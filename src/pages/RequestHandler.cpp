#include "RequestHandler.hpp"
#include <LittleFS.h>
#include <ctime>

namespace pages {

String getContentType(const char* name) {
	using mime::mimeTable; // from ESP8266WebServer
	const char* dot = std::strrchr(name, '.');
	if (dot) {
		if (strncasecmp_P(dot, ".bmp", 8)) {
			return String(FPSTR(WEB_CONTENT_TYPE_IMAGE_BMP));
		}
		for (size_t i = 0; i < mime::maxType; i++) {
			if (!strncasecmp_P(dot, mimeTable[i].endsWith, 8)) {
				return String(FPSTR(mimeTable[i].mimeType));
			}
		}
	}
	return String(FPSTR(mimeTable[mime::none].mimeType));
}

const char directoryIndexHeadingFormat[] PROGMEM = "<head><title>Index of %s</title></head><body><h1>Index of %s</h1><table><tr><th>Name</th><th>Modified time</th><th>Size</th></tr>";
const char directoryIndexEntryFormat[] PROGMEM = "<tr><td><a href=\"./%s\">%s</a></td><td>%s</td><td>%d B</td></tr>";
const char directoryIndexEmptyEntry[] PROGMEM = "<tr><td>(empty)</td><td></td><td></td></tr>";
const char directoryIndexFooter[] PROGMEM = "</table></body> ";

bool RequestHandler::handle(ESP8266WebServer& server, HTTPMethod method, const String& uri) {
	if constexpr (ensureHandlerCanHandleRequest) {
		if (!canHandle(method, uri)) 
			return false;
	}

	// TODO: move generic parts to separate base handler?

	switch (method) {
		case HTTP_OPTIONS: {
			server.sendHeader(PSTR("Allow"), PSTR("OPTIONS, HEAD, GET, POST, DELETE"));
			server.send(204);
			return true;
		}
		case HTTP_HEAD:
		case HTTP_GET: {
			// TODO: Implement ETAG mechanism: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/ETag
			if (LittleFS.exists(uri)) {
				File file = LittleFS.open(uri, "r");
				if (file) {
					if (file.isDirectory()) {
						// TODO: nicer and more configurable index view
						Dir dir = LittleFS.openDir(uri);
						size_t count = 0;
						size_t extraContentLength = 0;
						while (dir.next()) {
							count++;
							extraContentLength += dir.fileName().length() * 2;
							extraContentLength += baseTenDigits(dir.fileSize());
						}
						dir.rewind();

						server.setContentLength(
							sizeof(directoryIndexHeadingFormat) - 1 - 4 + uri.length() * 2 +
							(count == 0 ? sizeof(directoryIndexEmptyEntry) - 1 : 0) +
							count * (sizeof(directoryIndexEntryFormat) - 1 - 8) + extraContentLength +
							sizeof(directoryIndexFooter) - 1
						);
						server.send(200, WEB_CONTENT_TYPE_TEXT_HTML);

						if (method != HTTP_GET) {
							return true;
						}

						const size_t bufferLength = 256;
						char buffer[bufferLength];

						int writtenLength = snprintf_P(
							buffer, bufferLength,
							directoryIndexHeadingFormat,
							uri.c_str(), uri.c_str() // title & h1
						);
						server.sendContent(buffer, static_cast<size_t>(writtenLength));

						if (count == 0) {
							writtenLength = snprintf_P(
								buffer, bufferLength,
								directoryIndexEmptyEntry
							);
							server.sendContent(buffer, static_cast<size_t>(writtenLength));
						}
						else {
							char timeString[24];
							while (dir.next()) {
								std::time_t modifiedTime = dir.fileTime();
								std::strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", std::localtime(&modifiedTime));
								writtenLength = snprintf_P(
									buffer, bufferLength,
									directoryIndexEntryFormat,
									dir.fileName().c_str(), // href
									dir.fileName().c_str(), // link text
									timeString,
									dir.fileSize()
								);
								server.sendContent(buffer, static_cast<size_t>(writtenLength));
							}
						}

						writtenLength = snprintf_P(
							buffer, bufferLength,
							directoryIndexFooter
						);
						server.sendContent(buffer, static_cast<size_t>(writtenLength));
					}
					else /* file */ {
						server.setContentLength(file.size());
						server.send(200, getContentType(file.name()), emptyString);
						if (method == HTTP_GET) {
							file.sendAll(server.client());
						}
						return true;
					}
				}
				else /* could not open */ {
					server.send(500);
					return true;
				}
			}
			else /* does not exist */ {
				server.send(404);
				return true;
			}
		}
		case HTTP_POST: {
			if (errorCode)
				server.send(errorCode < 999 ? errorCode : 500);
			else if (uploadedFilesCount > 0)
				server.send(201);
			else
				server.send(400);

			// reset for future
			uploadedFilesCount = 0;
			errorCode = 0;

			return true;
		}
		default:
			server.send(501);
			return true;
	}
}

void RequestHandler::upload(ESP8266WebServer& server, const String& uri, HTTPUpload& upload) {
	if constexpr (ensureHandlerCanHandleRequest) {
		if (!canUpload(uri)) 
			return;
	}

	switch (upload.status) {
		case UPLOAD_FILE_START: {
			if (upload.type.equalsIgnoreCase(FPSTR(WEB_CONTENT_TYPE_IMAGE_BMP)))
				processingType = ProcessingType::Bitmap;
			else if (upload.type.equalsIgnoreCase(FPSTR(WEB_CONTENT_TYPE_APPLICATION_OCTET_STREAM)))
				processingType = ProcessingType::Configuration;
			else {
				LOG_DEBUG(pages, "Invalid content type");
				errorCode = 415;
				return;
			}

			if (upload.contentLength > 1024 * 6) {
				LOG_DEBUG(pages, "Too large");
				errorCode = 413;
				return;
			}

			std::string path = uri.c_str();
			if (LittleFS.exists(uri)) {
				File file = LittleFS.open(uri, "r");
				if (file) {
					if (file.isDirectory()) {
						if (!path.ends_with('/')) {
							path.append(1, '/');
						}
						path.append(server.upload().filename.c_str(), server.upload().filename.length());

						LOG_DEBUG(pages, "Adding to the directory");
					}
					else /* file */ {
						LOG_DEBUG(pages, "Overwriting existing file");
					}
				}
				else /* could not open */ {
					LOG_ERROR(FS, "Cannot open '%s' for read", uri);
					errorCode = 500;
					return;
				}
			}
			else /* does not exist */ {
				// path = uri;
			}

			LOG_DEBUG(pages, "Opening '%s' for saving", path.c_str());
			uploadedFile = LittleFS.open(path.c_str(), "w");

			if (processingType == ProcessingType::Bitmap) {
				bitmapProcessor.initialize();
			}

			break;
		}
		case UPLOAD_FILE_WRITE: {
			LOG_DEBUG(pages, "Processing upload");

			if (processingType == ProcessingType::Bitmap) {
				bitmapProcessor.chunk(upload.buf, upload.currentSize, uploadedFile);
			}
			else {
				uploadedFile.write(upload.buf, upload.currentSize);
			}
			
			break;
		}
		case UPLOAD_FILE_END: {
			if (processingType == ProcessingType::Bitmap) {
				bitmapProcessor.finish();
			}
			
			uploadedFile.close();
			uploadedFilesCount += 1;

			LOG_DEBUG(pages, "Upload saved");
			break;
		}
		case UPLOAD_FILE_ABORTED: {
			// Delete the unfinished file
			std::string path(uploadedFile.fullName()); // copy to avoid invalidation
			uploadedFile.close();
			LittleFS.remove(path.c_str());
			
			LOG_DEBUG(pages, "Upload aborted");
			break;
		}
	}
}

RequestHandler requestHandler;

}
