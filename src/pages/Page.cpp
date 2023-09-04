#include "Page.hpp"
#include "common.hpp"
#include <LittleFS.h>

namespace pages { 

void Page::loadById(uint8_t id) {
	char path[20];
	sprintf(path, "/pages/%u/config", id);
	load(path);
}
void Page::load(const char* path) {
	File file = LittleFS.open(path, "r");
	if (!file || !file.isFile()) {
		LOG_ERROR(Pages, "Failed to open '%s' as page config", path);
		return;
	}

	if (CHECK_LOG_LEVEL(Pages, LEVEL_DEBUG)) {
		LOG_DEBUG(Pages, "file size=%u", file.size());
	}

	int i = file.read(reinterpret_cast<uint8_t*>(this), sizeof(Page));
	LOG_DEBUG(Pages, "Loading page from '%s', read %u bytes", path, i);
	if (this->signature != Page::expectedSignature) {
		LOG_ERROR(Pages, "Invalid signature");
	}

	if (CHECK_LOG_LEVEL(Pages, LEVEL_DEBUG)) {
		LOG_DEBUG(Pages, "next=%u duration=%u", this->next, this->duration);
		if (this->usesBackgroundFromFile()) {
			LOG_DEBUG(Pages, "bg=%.16s d=%u", this->backgroundPath, this->backgroundDuration);
		}
		else {
			LOG_DEBUG(Pages, "bg=0x%04x d=%u", this->backgroundColors.primary, this->backgroundDuration);
		}

		for (uint8_t i = 0; i < Page::maxSprites; i++) {
			const auto& sprite = this->sprites[i];
			LOG_DEBUG(Pages, "sprite %u. type=%u x=%u y=%u", 
				i, sprite.common.type, sprite.common.x, sprite.common.y);

			switch (sprite.common.type) {
				case Sprite::Type::None:
					// Skip not used sprites
					break;
				case Sprite::Type::Text:
					LOG_DEBUG(Pages, "\tfont=%u dot=%u color=0x%04x text='%.18s'", 
						sprite.text.font, sprite.text.dotSize, sprite.text.color, sprite.text.text);
					break;
				case Sprite::Type::Time: {
					LOG_DEBUG(Pages, "\tfont=%u dot=%u color=0x%04x format='%.18s' utc=%u", 
						sprite.time.font, sprite.time.dotSize, sprite.time.color, sprite.time.format,
						sprite.time.useUTC);
					break;
				}
				case Sprite::Type::Temperature: {
					LOG_DEBUG(Pages, "\tfont=%u dot=%u degree=%u pad=%u source=%u future=%u prec=%u unit=%u", 
						sprite.temperature.font, sprite.temperature.dotSize, sprite.temperature.degreeSize, 
						sprite.temperature.padLeft, sprite.temperature.source, sprite.temperature.inFuture,
						sprite.temperature.precision, sprite.temperature.unit);
					// TODO: ref value/target colors
					break;
				}
				// TODO: other types
				// TODO: include full info/flags?
			}
		}

		// TODO: analog clock info
	}
}

}
