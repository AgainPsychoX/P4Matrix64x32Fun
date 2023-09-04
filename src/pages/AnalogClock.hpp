#pragma once

#include <cstdint>

namespace pages {

struct AnalogClock {
	// if X (or Y) == 255 == (uint8_t) -1 then not displayed
	uint8_t centerX = 255;
	uint8_t centerY;
	uint16_t centerColor;

	uint16_t hourArrowColor;
	uint16_t minuteArrowColor;
	uint16_t secondArrowColor;
	uint8_t hourArrowLength;
	uint8_t minuteArrowLength;
	uint8_t secondArrowLength; // 0 to disable
};

}
