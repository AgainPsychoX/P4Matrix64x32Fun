#include "bitmap.hpp"
#define PxMATRIX_double_buffer false
#include <PxMatrix.h>
#include <LittleFS.h>
#include <algorithm> // min, max

extern PxMATRIX display; // from main for drawToDisplay

namespace BMP {

/// Calculates required padding to ceil up to 4.
template <typename T>
inline uint8_t paddingToCeil4(T x) {
	return ((x % 4 > 0) ? (4 - x % 4) : 0);
}

void RGB565Converter::initialize() {
	height = 0;
	x = 0;
	y = 0;
	leftoverType = None;
	error = false;
}

bool RGB565Converter::chunk(const uint8_t* inputBuffer, size_t inputBufferLength, Stream& output) {
	if (error) return error;

	const uint8_t* inputPosition = inputBuffer;
	const uint8_t* const inputEnd = inputBuffer + inputBufferLength;

	if (!areHeadersProcessed()) {
		const uint8_t* headersBuffer = inputBuffer;
		if (inputBufferLength < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPV2INFOHEADER)) {
			LOG_DEBUG(BMP, "First chunk should contain full headers");
			return error = true;
			// TODO: use local buffer, and fill it till we have the headers ready
		}

		// Get and validate file header
		BITMAPFILEHEADER fileHeader;
		std::memcpy(&fileHeader, headersBuffer, sizeof(BITMAPFILEHEADER));
		if (fileHeader.signature != BMP::expectedSignature) {
			LOG_DEBUG(BMP, "Invalid signature");
			return error = true;
		}

		// Get and validate DIB header
		BITMAPV2INFOHEADER dibHeader;
		auto& headerSize = dibHeader.headerSize;
		headerSize = *reinterpret_cast<const uint32_t*>(headersBuffer);
		std::memcpy(&dibHeader, headersBuffer + sizeof(BITMAPFILEHEADER), static_cast<size_t>(headerSize));
		if (dibHeader.headerSize != 40) {
			LOG_DEBUG(BMP, "Unsupported header");
			return error = true;
		}
		if (dibHeader.width < 0 || dibHeader.height == 0) {
			LOG_DEBUG(BMP, "Invalid width or height");
			return error = true;
		}
		if (dibHeader.height < 0) {
			LOG_DEBUG(BMP, "Top-to-bottom rows order not supported");
			return error = true;
		}

		sourceBitsPerPixel = dibHeader.bitPerPixel;
		switch (dibHeader.bitPerPixel) {
			case 16: // target format, so just validate and pass to output
				break;
			case 24: // handled by converting to 16 bits
				if (dibHeader.compression != 0) {
					LOG_DEBUG(BMP, "Compression is not supported");
					return error = true;
				}
				break;
			default:
				LOG_DEBUG(BMP, "24 bits per pixel expected");
				return error = true;
		}

		// Move input buffer position to the pixels
		inputPosition += fileHeader.offsetToPixelArray;

		// Despite header being in fact 52, the field needs to be 40 for Windows to understand it
		dibHeader.headerSize = 40; // BITMAPINFOHEADER
		// dibHeader.headerSize = 52; // BITMAPV2INFOHEADER

		// Prepare BMP header for the output
		dibHeader.imageSize = (dibHeader.width * dibHeader.height) * 2; // 16 bits
		fileHeader.offsetToPixelArray = sizeof(fileHeader) + sizeof(dibHeader);
		fileHeader.size = fileHeader.offsetToPixelArray + dibHeader.imageSize; // TODO: should it include padding?
		dibHeader.bitPerPixel = 16;
		dibHeader.compression = 3; // signal RGB masks should be used (BI_BITFIELDS)
		dibHeader.redMask = 0xF800;
		dibHeader.greenMask = 0x07E0;
		dibHeader.blueMask = 0x001F;

		if (sourceBitsPerPixel == 16) {
			// Validate headers
			if (std::memcmp(&fileHeader, headersBuffer, sizeof(fileHeader)) != 0 ||
				std::memcmp(&dibHeader, headersBuffer + sizeof(fileHeader), sizeof(dibHeader)) != 0
			) {
				LOG_DEBUG(BMP, "16 bits per pixel input header validation failed");
				return error = true;
			}
		}

		// Write headers to output
		output.write(reinterpret_cast<uint8_t*>(&fileHeader), sizeof(fileHeader));
		output.write(reinterpret_cast<uint8_t*>(&dibHeader), sizeof(dibHeader));

		// Save width & height to be used in pixels converting
		width = static_cast<axis_index_t>(dibHeader.width);
		height = static_cast<axis_index_t>(dibHeader.height);

		// Calculate padding (rows need to be aligned to 4 bytes)
		inputRowPadding = paddingToCeil4(width * 3 /* 24 bits */);
		outputRowPadding = paddingToCeil4(width * 2 /* 16 bits */);
	}

	if (sourceBitsPerPixel == 16) {
		// Pass input to output directly
		output.write(inputBuffer, sizeof(inputBufferLength));
		return error; // no converting
	}

	// Convert and write pixels
	for (/* rows */ ;y < height; y++) {
		if (leftoverType == Padding) [[unlikely]] {
			// Omit remaining bytes of the padding
			while (leftoverLength < inputRowPadding) {
				if (inputPosition >= inputEnd) [[unlikely]] {
					return error; // wait for next chunk
				}
				leftoverLength++;
			}

			// Move to next row (as current finished), if any
			y++;
			if (y >= height) {
				return error; // no more chunks
			}
		}

		for (/* pixels in rows */; x < width; x++) {
			rgb888be_t rgb888be;
			
			if (leftoverType == Pixel) [[unlikely]] {
				// Collect remaining bytes for the pixel
				while (leftoverLength < sizeof(rgb888be_t)) {
					if (inputPosition >= inputEnd) [[unlikely]] {
						return error; // wait for next chunk
					}
					leftoverData.bytes[leftoverLength++] = *inputPosition++;
				}
				rgb888be = leftoverData.pixel;
				leftoverType = None;
			}
			else /* get pixel from input */ {
				if (inputPosition + sizeof(rgb888be_t) > inputEnd) [[unlikely]] {
					// Save the leftover
					leftoverType = Pixel;
					leftoverLength = 0;
					while (inputPosition < inputEnd) {
						leftoverData.bytes[leftoverLength++] = *inputPosition++;
					}

					return error; // wait for next chunk
				}
				rgb888be = *reinterpret_cast<const rgb888be_t*>(inputPosition);
				inputPosition += sizeof(rgb888be_t);
			}

			// Convert RGB888 to RGB565
			uint16_t rgb565 = (
				(static_cast<uint16_t>(rgb888be.r >> 3) << 11) | 
				(static_cast<uint16_t>(rgb888be.g >> 2) << 5) | 
				(static_cast<uint16_t>(rgb888be.b >> 3))
			);

			// Write converted pixel to output
			output.write(reinterpret_cast<char*>(&rgb565), sizeof(rgb565));
		}

		// Add row padding to output
		if (outputRowPadding) {
			output.write("AAAA", outputRowPadding);
		}

		// Skip row padding of input
		if (inputRowPadding) {
			if (inputPosition + inputRowPadding > inputEnd) [[unlikely]] {
				// Mark as the leftover
				leftoverType = Padding;
				leftoverLength = static_cast<uint8_t>(inputEnd - inputPosition);

				return error; // wait for next chunk
			}
			inputPosition += inputRowPadding;
		}
	}

	return error;
}

bool RGB565Converter::finish() {
	if (error) return error;

	if (x != width || y != height || leftoverType == Pixel) {
		LOG_DEBUG(BMP, "Unexpected end");
		// TODO: fill remaining pixels with zero to allow soft-error?
		return error = true;
	}

	return error;
}

bool drawToDisplay(Stream& file, uint8_t targetX, uint8_t targetY, uint16_t transparentColor) {
	BITMAPFILEHEADER fileHeader;
	file.read(reinterpret_cast<uint8_t*>(&fileHeader), sizeof(BITMAPFILEHEADER));
	if (fileHeader.signature != BMP::expectedSignature) {
		LOG_DEBUG(BMP, "Invalid signature");
		return false;
	}

	BITMAPV2INFOHEADER dibHeader;
	file.read(reinterpret_cast<uint8_t*>(&dibHeader), sizeof(BITMAPV2INFOHEADER));
	if (dibHeader.headerSize != 40) {
		LOG_DEBUG(BMP, "Unsupported header");
		return false;
	}
	if (dibHeader.width < 0 || dibHeader.height == 0) {
		LOG_DEBUG(BMP, "Invalid width or height");
		return false;
	}
	if (dibHeader.height < 0) {
		LOG_DEBUG(BMP, "Top-to-bottom rows order not supported");
		return false;
	}
	if (dibHeader.bitPerPixel != 16) {
		LOG_DEBUG(BMP, "16 bits per pixel expected");
		return false;
	}

	// Draw pixels (bottom-to-top per BMP standard)
	const size_t rowLengthInBytes = dibHeader.width * sizeof(uint16_t) + paddingToCeil4(dibHeader.width);
	uint16_t* rowBuffer = new uint16_t[dibHeader.width + 2 /* account for padding too*/];
	const auto xLimit = std::min<uint8_t>(targetX + dibHeader.width, display.width());
	const auto yLimit = std::min<uint8_t>(targetY + dibHeader.height, display.height());
	for (uint8_t y = dibHeader.height; y > yLimit; y--) {
		// Skip rows that are always outside the display
		file.read(reinterpret_cast<uint8_t*>(&rowBuffer), rowLengthInBytes);
	}
	display.startWrite();
	for (int16_t y = yLimit; y > targetY; y--) { // needs to be signed in case targetY = 0
		file.read(reinterpret_cast<uint8_t*>(&rowBuffer), rowLengthInBytes);
		for (uint8_t x = 0; x < xLimit; x++) {
			uint16_t color = *(rowBuffer + x);
			if (transparentColor && transparentColor == color) {
				continue;
			}
			display.writePixel(targetX + x, y, color);
		}
	}
	display.endWrite();

	return true;
}

}
