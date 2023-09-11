#pragma once

#include <cstdint>
#include <cstring> // strncpy
#include "utils.hpp" // STRUCT_PADDING_BYTES
#include "colors.hpp"

namespace pages {

union Sprite {
	enum class Type : uint8_t {
		None,       // sprite not used
		Text,       // simple text
		Time,       // uses std::strftime with custom extensions with local time
		Temperature,// uses temperature info (local or online)
		Image,      // includes path to BMP or animation file
		CustomChar, // allows for custom characters (1-bit image)
	};

	struct Common {
		STRUCT_PADDING_BYTES(24 - 2 - 1);
		Sprite::Type type = Sprite::Type::None;
		uint8_t x;
		uint8_t y;
	} common;
	static_assert(sizeof(Common) == 24);

	// Text sprite properties
	struct Text {
		char text[24 - 2 - 4];
		inline void setText(const char* newText) {
			std::strncpy(text, newText, sizeof(text));
			text[sizeof(text) - 1] = 0;
		}

		uint16_t color = colors::to565(colors::white);
		uint8_t font : 4 = 0; // default 6x8
		uint8_t dotSize : 2 = 0; // 0 for font-based (monospace!)
		STRUCT_PADDING_BITS(2);

		Sprite::Type type = Sprite::Type::Text;
		uint8_t x;
		uint8_t y;
	} text;
	static_assert(sizeof(Text) == 24);

	// Time-based text sprite properties
	struct Time {
		char format[16];
		inline void setFormat(const char* newFormat) {
			std::strncpy(format, newFormat, sizeof(format));
			format[sizeof(format) - 1] = 0;
		}

		bool useUTC : 1 = false; // true for UTC time, false for local timezone
		bool blinkColon : 1 = true; // blink on new second for 500ms
		bool bothColons : 1 = true; // if false, only last colon will blink
		bool blinkSlow : 1 = true; // if true, change state every second
		STRUCT_PADDING_BITS(4);

		uint8_t colonWidthStart : 4 = 0; // skips up to 15 pixels from colon start
		uint8_t colonMinusAdvanceX : 4 = 0; // shortens colon advance in X axis

		uint16_t color = colors::to565(colors::white);
		uint8_t font : 4 = 0;
		uint8_t dotSize : 2 = 0; // 0 for font-based (monospace!)
		STRUCT_PADDING_BITS(2);

		Sprite::Type type = Sprite::Type::Time;
		uint8_t x;
		uint8_t y;
	} time;
	static_assert(sizeof(Time) == 24);

	// Temperature-based text sprite properties (format is always "%.*f")
	struct Temperature {
		enum class Source : uint8_t { 
			Local, 
			OnlineHour,
			OnlineDay,
			OnlineNight,
		};

		enum Unit : uint8_t {
			Kelvin,
			Celsius,
			Fahrenheit,
		};

		// Reference temperatures/target colors should be kept sorted.

		float referenceValue[3] = { 5.f, 15.f, 25.0f };
		uint16_t targetColors[3] = {
			colors::to565(colors::RGB {0, 0, 255}),
			colors::to565(colors::RGB {0, 255, 0}),
			colors::to565(colors::RGB {255, 0, 0})
		};
		// TODO: if 3 references/targets should be ever used, 16 bits (2 bytes)
		//  half-float-precision could be used; or even 255.99 using 2 bytes.

		inline void setSingleColor(uint16_t color) {
			targetColors[0] = targetColors[1] = targetColors[2] = color;
		}
		inline void setGradient(
			float t0, uint16_t c0, 
			float t1, uint16_t c1
		) {
			referenceValue[0] = t0; targetColors[0] = c0;
			referenceValue[1] = t1; targetColors[1] = targetColors[2] = c1;
		}
		inline void setGradient(
			float t0, uint16_t c0,
			float t1, uint16_t c1,
			float t2, uint16_t c2
		) {
			referenceValue[0] = t0; targetColors[0] = c0;
			referenceValue[1] = t1; targetColors[1] = c1;
			referenceValue[2] = t2; targetColors[2] = c2;
		}

		bool showUnit : 1 = true;
		bool showDegree : 1 = true;
		Unit unit : 2 = Celsius;
		uint8_t precision : 2 = 1;
		STRUCT_PADDING_BITS(2);

		bool padLeft : 1 = false; // true to add space if temperature is single digit
		Source source : 2 = Source::Local;
		uint8_t inFuture : 5 = 0; // if online source used, select point n-units ahead

		uint8_t font : 4 = 0;
		uint8_t dotSize : 2 = 0; // 0 for font-based (monospace!)
		uint8_t degreeSize : 2 = 0; // 0 for font-based (monospace!) as single quote

		Sprite::Type type = Sprite::Type::Temperature;
		uint8_t x;
		uint8_t y;

		uint16_t interpolateColor(float temperature);
	} temperature;
	static_assert(sizeof(Temperature) == 24);

	// Image/animation sprite properties
	struct Image {
		char path[16];
		inline void setPath(const char* newPath) {
			std::strncpy(path, newPath, sizeof(path));
			path[sizeof(path) - 1] = 0;
		}

		uint16_t transparentColor = 0; // (0 means no transparency)
		uint16_t frameDuration = 0; // >0 ms for animation, or 0 for still image

		STRUCT_PADDING_BYTES(1);
		Sprite::Type type = Sprite::Type::Image;
		uint8_t x;
		uint8_t y;
	} image;
	static_assert(sizeof(Image) == 24);

	// Custom character (aka inline 1-bit image/bitmap)
	struct CustomChar {
		uint8_t data[24 - 2 - 4];
		uint16_t color = colors::to565(colors::white);
		uint8_t width; // width of the char; height is deduced by dividing
		inline uint8_t height() const { return static_cast<uint8_t>(sizeof(data) * 8) / width; }
		Sprite::Type type = Sprite::Type::CustomChar;
		uint8_t x;
		uint8_t y;
	} customChar;
	static_assert(sizeof(CustomChar) == 24);
};
constexpr auto _sizeof_Sprite = sizeof(Sprite);
static_assert(sizeof(Sprite) == 24);

}
