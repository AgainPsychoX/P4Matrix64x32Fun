#pragma once

#include <cstdint>
#include "utils.hpp"

namespace pages {

struct Animation {
	static constexpr uint16_t expectedSignature = 0x4150; // '4A'
	uint16_t signature = expectedSignature;
	
	static constexpr size_t maxFrames = 12;
	struct Frame {
		char path[18];
		uint16_t duration; // ms
	} frames[maxFrames];
};
constexpr auto _sizeof_Animation = sizeof(Animation);
static_assert(sizeof(Animation) <= 256);

}
