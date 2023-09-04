#include <iostream>
#include <fstream>
#include <cstdint>
#include "endianness.hpp"

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

int main(int argc, char* argv[]) {
	std::ifstream input(argc > 1 ? argv[1] : "input.bmp", std::ios::binary);
	std::ofstream output(argc > 2 ? argv[2] : "output.bmp", std::ios::binary);

	if (!input.is_open() || !output.is_open()) {
		std::cerr << "Error opening files." << std::endl;
		return 1;
	}

	BITMAPFILEHEADER fileHeader;
	input.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
	if (fileHeader.signature != 0x4D42) {
		std::cerr << "BMP signature expected." << std::endl;
		return 1;
	}

	BITMAPV2INFOHEADER dibHeader;
	auto& headerSize = dibHeader.headerSize;
	input.read(reinterpret_cast<char*>(&headerSize), sizeof(headerSize));
	switch (headerSize) {
		case 40:
		case 52: {
			std::cout << "Header size: " << headerSize << std::endl;
			
			// Read remaining fields (aside from header size)
			input.read(
				reinterpret_cast<char*>(&headerSize) + sizeof(headerSize), 
				headerSize - sizeof(headerSize)
			);
			break;
		}
		default: {
			std::cerr << "Header size not supported." << std::endl;
			return 1;
		}
	}
	
	// Despite header being in fact 52, it needs to be 40 for Windows to understand it
	dibHeader.headerSize = 40; // BITMAPINFOHEADER
	// dibHeader.headerSize = 52; // BITMAPV2INFOHEADER

	// Print some data about the input file
	std::cout << "Width: " << dibHeader.width << std::endl;
	std::cout << "Height: " << dibHeader.height << std::endl;

	if (dibHeader.width < 0 || dibHeader.height == 0) {
		std::cerr << "Invalid width or height." << std::endl;
		return 1;
	}
	if (dibHeader.height < 0) {
		std::cerr << "Top-to-bottom rows order not supported." << std::endl;
		return 1;
	}
	if (dibHeader.bitPerPixel != 24) {
		std::cerr << "24 bits per pixel expected, other not supported." << std::endl;
		return 1;
	}
	if (dibHeader.compression != 0) {
		std::cerr << "Compression is not supported." << std::endl;
		return 1;
	}

	// Update BMP header for the output file
	dibHeader.imageSize = (dibHeader.width * dibHeader.height) * 2; // 16 bits
	fileHeader.offsetToPixelArray = sizeof(fileHeader) + sizeof(dibHeader);
	fileHeader.size = fileHeader.offsetToPixelArray + dibHeader.imageSize; // TODO: should it include padding?
	dibHeader.bitPerPixel = 16;
	dibHeader.compression = 3; // signal RGB masks should be used (BI_BITFIELDS)
	dibHeader.redMask = 0xF800;
	dibHeader.greenMask = 0x07E0;
	dibHeader.blueMask = 0x001F;

	// Write headers to output file
	output.write(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
	output.write(reinterpret_cast<char*>(&dibHeader), sizeof(dibHeader));

	// Convert and write pixel data
	const size_t inputRowLength = static_cast<size_t>(dibHeader.width) * 3; // 24 bits
	const size_t inputRowPadding = (inputRowLength % 4 > 0) ? (4 - inputRowLength % 4) : 0; // align to 4 bytes
	const size_t outputRowLength = static_cast<size_t>(dibHeader.width) * 2; // 16 bits
	const size_t outputRowPadding = (outputRowLength % 4 > 0) ? (4 - outputRowLength % 4) : 0; // align to 4 bytes
	const std::string outputRowPaddingString = std::string(outputRowLength, 'A');
	struct {
		uint8_t b;
		uint8_t g;
		uint8_t r;
	} rgb888be;
	uint16_t rgb565;
	for (int32_t y = 0; y < dibHeader.height; y++) {
		for (int32_t x = 0; x < dibHeader.width; x++) {
			// Read RGB888 (24 bits)
			if (!input.read(reinterpret_cast<char*>(&rgb888be), sizeof(rgb888be))) {
				std::cerr << "Data exhausted before expected end." << std::endl;
				return 1;
			}

			// Convert to RGB565 (16 bits)
			uint16_t r = static_cast<uint16_t>(rgb888be.r >> 3);
			uint16_t g = static_cast<uint16_t>(rgb888be.g >> 2);
			uint16_t b = static_cast<uint16_t>(rgb888be.b >> 3);
			rgb565 = ((r << 11) | (g << 5) | b);

			// Write the converted pixel to the output file
			output.write(reinterpret_cast<char*>(&rgb565), sizeof(rgb565));
		}

		// Add row padding to output
		if (outputRowPadding) {
			output << outputRowPaddingString;
		}

		// Skip row padding of input
		if (inputRowPadding && !input.ignore(inputRowPadding)) {
			if (y == dibHeader.height - 1) {
				break; // Ignore missing padding on very end
			}
			std::cerr << "Data exhausted before expected end." << std::endl;
			return 1;
		}
	}

	input.close();
	output.close();

	std::cout << "Conversion completed." << std::endl;

	return 0;
}
