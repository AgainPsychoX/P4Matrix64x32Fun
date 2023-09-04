#pragma once

#include <cstdint>
#include "utils.hpp"
#include "AnalogClock.hpp"
#include "Sprite.hpp"

namespace pages {

struct Page {
	static constexpr uint16_t expectedSignature = 0x5034; // '4P'
	uint16_t signature = expectedSignature;

	uint8_t _reserved;
	uint8_t next; // ID/number of next page to be displayed after this one (when duration runs out)
	uint16_t duration = 0; // Duration in milliseconds to display the page for, or 0 for always (no next page)
	inline bool hasNextPage() const { return duration != 0; }

	union {
		char backgroundPath[16];
		struct {
			uint8_t flag; // '\0' if color used, '/' if file (part of path)
			STRUCT_PADDING_BYTES(1);
			uint16_t primary;

			inline void setPrimary(uint16_t value) {
				flag = 0;
				primary = value;
			}
		} backgroundColors;
	};
	inline bool usesBackgroundFromFile() const { return backgroundColors.flag == '/'; }
	uint16_t backgroundDuration; // Duration per frame/slide in milliseconds, or 0 to update on full updates only

	AnalogClock analog;

	static constexpr size_t maxSprites = 9;
	Sprite sprites[maxSprites];

	void loadById(uint8_t id);
	void load(const char* path);
};
constexpr auto _sizeof_Page = sizeof(Page);
static_assert(sizeof(Page) <= 256);

}
