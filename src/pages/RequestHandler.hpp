#pragma once

#include "common.hpp"
#include "bitmap.hpp"

namespace pages {

/// \brief Request handler for web server to handle viewing and modifying
/// pages-to-be-displayed settings. 
/// Uses file system for storing config (as binary) and assets (BMP) files.
class RequestHandler : public ESP8266WebServer::RequestHandlerType {
	/// Checking whenever given handler can be used should be already done 
	/// by request parsing when selecting right handler, but allow double checking.
	static constexpr bool ensureHandlerCanHandleRequest = false;

public:
	bool canHandle(HTTPMethod requestMethod, const String& requestUri) override {
		return requestUri.startsWith(F("/pages"));
	}

	bool canUpload(const String& requestUri) override {
		return canHandle(HTTP_POST, requestUri);
	}

	bool handle(ESP8266WebServer& server, HTTPMethod requestMethod, const String& requestUri) override;

	void upload(ESP8266WebServer& server, const String& requestUri, HTTPUpload& upload) override;

protected:
	enum class ProcessingType : uint8_t {
		None,
		Bitmap, // image/bmp; to be processed and saved as 16 bits per pixel BMP
		Configuration, // application/octet-stream; should confirm to `Page` struct
	};

	File uploadedFile;
	uint8_t uploadedFilesCount = 0;
	ProcessingType processingType;
	BMP::RGB565Converter bitmapProcessor; // TODO: dynamicly allocate & RAII

	int errorCode = 0; // used to pass upload result info to `handle`
};

extern RequestHandler requestHandler;

}
