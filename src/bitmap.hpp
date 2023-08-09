#pragma once

#include "common.hpp"

namespace BMP {

#pragma pack(push, 1)
struct BITMAPFILEHEADER {
	uint16_t signature;
	uint32_t size;
	uint16_t reserved1;
	uint16_t reserved2;
	uint32_t offsetToPixelArray;
};
struct BITMAPINFOHEADER {
	uint32_t headerSize = 40;
	int32_t width;
	int32_t height;
	uint16_t planes;
	uint16_t bitPerPixel;
	uint32_t compression;
	uint32_t imageSize;
	int32_t xResolution;
	int32_t yResolution;
	uint32_t colorsUsed;
	uint32_t colorsImportant; 
};
struct BITMAPV2INFOHEADER : BITMAPINFOHEADER {
	BITMAPV2INFOHEADER() {
		headerSize = 52;
	}

	uint32_t redMask;
	uint32_t greenMask;
	uint32_t blueMask;
};
#pragma pack(pop)

struct rgb888be_t {
	uint8_t b;
	uint8_t g;
	uint8_t r;
};

/// \brief Coordinates chunked conversion of 24 bit (RGB888) BMP file 
/// to 16 bit (RGB565) format. 
class RGB565Converter
{
	using foo_t = int16_t;

	foo_t width;
	foo_t height;
	foo_t x;
	foo_t y;
	uint8_t inputRowPadding;
	uint8_t outputRowPadding;
	
	enum LeftoverType { None, Pixel, Padding };

	union {
		uint8_t bytes[sizeof(rgb888be_t)]; // used to (partially) fill pixel data
		rgb888be_t pixel;
	} leftoverData;
	uint8_t leftoverLength;
	LeftoverType leftoverType;

	uint8_t sourceBitsPerPixel;

	bool error;

	inline bool areHeadersProcessed() {
		return height != 0;
	}

public:
	// TODO: refactor to satisfy RAII
	// TODO: error checking by `good()` & `operator bool()` instead return values?
	// TODO: consider checking output.write result to confirm all bytes were written?
	// TODO: refactor chunk into smaller functions? like `processHeaders` and `convertFrom888`?

	void initialize();

	bool chunk(const uint8_t* inputBuffer, size_t inputBufferLength, Stream& output);

	bool finish();
};

}
