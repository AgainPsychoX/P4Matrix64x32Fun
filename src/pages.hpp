#pragma once

#include "common.hpp"
#include "bitmap.hpp"

#define STRUCT_PADDING_BYTES(size) private: uint8_t ANONYMOUS_VARIABLE(_pad_)[size]; public:
#define STRUCT_PADDING_BITS(size) private: uint8_t ANONYMOUS_VARIABLE(_pad_) : size; public:

namespace PageConfiguration { 

struct AnalogClock {
	// if X (or Y) == 255 == (uint8_t) -1 then not displayed
	uint8_t centerX;
	uint8_t centerY;
	uint16_t centerColor;

	uint16_t hourArrowColor;
	uint16_t minuteArrowColor;
	uint16_t secondArrowColor;
	uint8_t hourArrowLength;
	uint8_t minuteArrowLength;
	uint8_t secondArrowLength; // 0 to disable
};

enum class SpriteType : uint8_t {
	None,       // sprite not used
	Text,       // simple text
	Image,      // includes path to BMP file
	Animation,  // includes path to directory with 0..N.bmp files
	Time,       // uses std::strftime with custom extensions with local time
	Temperature,// uses temperature info 
	CustomChar, // allows for custom characters (1-bit image)
};


union Sprite {
	// Text sprite properties
	struct Text {
		char text[32 - 2 - 4];
		uint16_t color;
		uint8_t font : 4;
		uint8_t dotSize : 2; // 0 for font-based (monospace!)
		STRUCT_PADDING_BITS(2);
		STRUCT_PADDING_BYTES(3); // type & x, y
	} text;
	static_assert(sizeof(text) == 32);

	// Image/animation sprite properties
	struct {
		char path[32 - 2 - 4];
		uint16_t transparentColor; 
		uint8_t frameDuration; // ms / 33, or 0 to use from file
		STRUCT_PADDING_BYTES(3); // type & x, y
	} file;
	static_assert(sizeof(file) == 32);

	// Time-based text sprite properties
	struct Time {
		char format[24];

		uint8_t font : 4;
		uint8_t dotSize : 2; // 0 for font-based (monospace!)
		STRUCT_PADDING_BITS(2);

		bool useUTC : 1; // true for UTC time, false for local timezone
		bool blinkColon : 1; // blink on new second for 500ms
		bool bothColons : 1; // if false, only last colon will blink
		bool blinkSlow : 1; // if true, change state every second
		STRUCT_PADDING_BITS(4);

		uint8_t colonWidthStart; // skips up to 15 pixels from colon start
		uint8_t colonMinusAdvanceX; // shortens colon advance in X axis

		STRUCT_PADDING_BYTES(1);
		STRUCT_PADDING_BYTES(3); // type & x, y
	} time;
	static_assert(sizeof(time) == 32);

	// Temperature-based text sprite properties (format is always "%.*f")
	struct Temperature {
		enum class Source : uint8_t { 
			Local, 
			OnlineHour,
			OnlineDay,
			OnlineNight,
		};

		// Reference temperatures/target colors should be kept sorted.

		float referenceTemperatures[4];
		uint16_t targetColors[4];

		uint8_t font : 4;
		uint8_t dotSize : 2; // 0 for font-based (monospace!)
		uint8_t degreeSize : 2; // 0 for font-based (monospace!) as single quote

		bool padLeft : 1; // true to add space if temperature is single digit
		Source source : 2;
		uint8_t inFuture : 5; // if online source used, select day n-days ahead

		STRUCT_PADDING_BYTES(3);
		STRUCT_PADDING_BYTES(3); // type & x, y

		uint16_t interpolateColor(float temperature);
	} temperature;
	static_assert(sizeof(temperature) == 32);

	// Custom character (aka inline 1-bit image/bitmap)
	struct CustomChar {
		uint8_t data[32 - 2 - 4];
		uint16_t color;
		uint8_t width; // width of the char; height is deduced by dividing
		STRUCT_PADDING_BYTES(3); // type & x, y
	} customChar;
	static_assert(sizeof(customChar) == 32);
	
	struct Common {
		STRUCT_PADDING_BYTES(32 - 2 - 1);
		SpriteType type;
		uint8_t x;
		uint8_t y;
	} unknown;
	static_assert(sizeof(unknown) == 32);
};
constexpr auto _sizeof_Sprite = sizeof(Sprite);
static_assert(sizeof(Sprite) == 32);

enum class BackgroundMode : uint8_t {
	None,
	Simple,  // path format: /pages/0.bmp
	Slides,  // path format: /pages/0/0.bmp
	Weather, // path format: /pages/0/thunderstorm/0.bmp
	Month,   // path format: /pages/0/september/0.bmp
	Season,  // path format: /pages/0/winter/0.bmp
};

struct Page {
	BackgroundMode backgroundMode;
	uint8_t next; // ID/number of next page to be displayed after this one
	uint16_t duration; // how long to display the page, or 0 for always

	/// Duration per frame in milliseconds, or 0 to read the duration 
	/// from BMP file via reused reserved field in file header.
	uint16_t backgroundFrameDuration; 

	AnalogClock analog;

	Sprite sprites[7];
};
constexpr auto _sizeof_Page = sizeof(Page);
static_assert(sizeof(Page) <= 256);

////////////////////////////////////////////////////////////////////////////////

const char* mimeTypeForFile(const char* name);

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
		Bitmap, // image/bmp
		Configuration, // application/octet-stream
	};

	File uploadedFile;
	uint8_t uploadedFilesCount = 0;
	ProcessingType processingType;
	BMP::RGB565Converter bitmapProcessor; // TODO: dynamicly allocate & RAII
	int errorCode;
};

extern RequestHandler requestHandler;

}
