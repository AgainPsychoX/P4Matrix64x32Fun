// Small C++ tool/script to generate example `Page` configuration binary files.
// Compile with: 
// $ gcc main.cpp -o out.exe -I '../../src' -std=gnu++20 -lstdc++

#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstring>
#include "colors.hpp"
#include "pages/Page.hpp"

using namespace colors;
using namespace pages;

// TODO: make it Qt app to practice Qt?

Sprite textSprite(uint8_t x, uint8_t y, const char* text, uint16_t color = 0xFFFF, uint8_t font = 0) {
	Sprite sprite{};
	sprite.common.type = Sprite::Type::Text;
	sprite.common.x = x;
	sprite.common.y = y;
	sprite.text.setText("T:");
	sprite.text.color = color;
	sprite.text.font = font;
	return sprite;
}

int main(int argc, char* argv[]) {
	Page page {
		.duration = 0, // single page
		.backgroundPath = "/assets/wac.bmp",
		.backgroundDuration = 0, // single frame anyway
		.analog = {
			.centerX = 17,
			.centerY = 17,
			.centerColor = to565(white),
			.hourArrowColor = to565(RGB {34, 177, 76}),
			.minuteArrowColor = to565(RGB {181, 230, 29}),
			.secondArrowColor = to565(RGB {255, 201, 14}),
			.hourArrowLength = 5,
			.minuteArrowLength = 7,
			.secondArrowLength = 9,
		},
		.sprites = {
			// textSprite(32, 0, "T:"),
			{ .text = { .text = "T:", .x = 32, .y = 0, } },
			{ .temperature = {
				.referenceValue = { 15.0f, 25.0f, 35.0f },
				.targetColors = { to565(RGB {0, 0, 255}), to565(RGB {0, 255, 0}), to565(RGB {255, 0, 0}) },
				.x = 32 + 12, .y = 0, 
			} },
			{ .time = { .format = "%T", .x = 32, .y = 25 } },
		}
	};
	// for (auto& sprite : page.sprites) {
	// 	sprite.common.type = Sprite::Type::None;
	// }

	// page.sprites[0] = textSprite(32, 0, "T:");

	// page.sprites[1].common.type = Sprite::Type::Temperature;
	// page.sprites[1].common.x = 32 + 12;
	// page.sprites[1].common.y = 0;
	// page.sprites[1].temperature.setGradient(
	// 	5.0f + 10,  to565(RGB {0, 0, 255}), // +10 ref temp for easier testing
	// 	15.0f + 10, to565(RGB {0, 255, 0}),
	// 	25.0f + 10, to565(RGB {255, 0, 0})
	// );
	// page.sprites[1].temperature.dotSize = 1;
	// page.sprites[1].temperature.degreeSize = 1;

	// page.sprites[2].common.type = Sprite::Type::Time;
	// page.sprites[2].common.x = 32;
	// page.sprites[2].common.y = 25;
	// page.sprites[2].time.setFormat("%T");

	std::ofstream output(argc > 1 ? argv[1] : "output.bin", std::ios::binary);
	output.write(reinterpret_cast<char*>(&page), sizeof(page));
	output.close();

	return 0;
}
