#include "common.hpp"
#define PxMATRIX_double_buffer false
#include <PxMatrix.h>
#include <Fonts/FreeSerifBold12pt7b.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <DallasTemperature.h> // for DS18B20 thermometer
#include <LittleFS.h>
#include "Network.hpp"
#include "NTP.hpp"
#include "pages/Page.hpp"
#include "pages/Animation.hpp"
#include "pages/RequestHandler.hpp"
#include "webEncoded/WebStaticContent.hpp"

////////////////////////////////////////////////////////////////////////////////

// Pins & width for the display
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2
#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 32
#define DISPLAY_INTERVAL 5
#define DISPLAY_SHOW_TIME 30

Ticker displayTicker;
PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D);
#define TEST_COLORS_ON_START 0

////////////////////////////////////////////////////////////////////////////////

Settings* settings;
ESP8266WebServer webServer(80);
bool showIP = true;
constexpr millis_t showIPtimeout = 20000;

////////////////////////////////////////////////////////////////////////////////

OneWire oneWire;
DallasTemperature oneWireThermometers(&oneWire);
float temperature = 0; // avg of last and current read (simplest noise reduction)

////////////////////////////////////////////////////////////////////////////////

const GFXfont* fontById(uint8_t font) {
	switch (font) {
		// case 1:  return &FreeMono9pt7b;
		// case 2:  return &FreeMono12pt7b;
		// case 3:  return &FreeMonoBold9pt7b;
		// case 4:  return &FreeMonoBold12pt7b;
		// case 5:  return &FreeSans9pt7b;
		// case 6:  return &FreeSans12pt7b;
		// case 7:  return &FreeSansBold9pt7b;
		// case 8:  return &FreeSansBold12pt7b;
		// case 9:  return &FreeSerif9pt7b;
		// case 10: return &FreeSerif12pt7b;
		// case 11: return &FreeSerifBold9pt7b;
		case 12: return &FreeSerifBold12pt7b;
		// case 13: return &Picopixel
		// case 14: return &Org_01
		// case 15: return &TomThumb
		default: return nullptr; // default 6x8 font will be used
	}
}

// TODO: refactor/encapsulate stuff into pages manager of some sort?
pages::Page activePage;
millis_t lastPageChange;

millis_t lastBackgroundFrame;
millis_t lastSpriteFrame[pages::Page::maxSprites];
uint8_t backgroundFrameIndex;
uint8_t spriteFrameIndex[pages::Page::maxSprites];

/// Setups default active page, most likely used in case the first page fails to load
inline void prepareDefaultActivePage() {
	using namespace pages;
	activePage.backgroundColors.setPrimary(colors::to565(colors::RGB {38, 13, 30}));
	activePage.sprites[0].common.type = Sprite::Type::Text;
	activePage.sprites[0].common.x = 7;
	activePage.sprites[0].common.y = 7;
	activePage.sprites[0].text.color = colors::to565(colors::white);
	activePage.sprites[0].text.font = 0; // default font
	activePage.sprites[0].text.setText("FS FAIL?");
}

void changeActivePage(uint8_t id) {
	activePage.loadById(id);
	lastPageChange = millis();
}

void substitutePathVariables(char* output, const char* raw) {
	for (const char* fp = raw; *fp; fp++) {
		if (*fp == '$') {
			fp++;
			switch (*fp) {
				case 'M': /* month*/ {
					char buffer[16];
					std::time_t time = std::time({});
					std::strftime(buffer, sizeof(buffer), "%B", std::localtime(&time));
					buffer[0] += ('a' - 'A');
					const char* vp = buffer;
					while (*vp) *output++ = *vp++;
					break;
				}
				case 'S': /* season */ {
					std::time_t time = std::time({});
					int m = std::localtime(&time)->tm_mon;
					const char* vp = "winter";
					if (m > 1) {
						/**/ if (m < 5) vp = "spring";
						else if (m < 8) vp = "summer";
						else if (m < 11) vp = "fall";
					}
					while (*vp) *output++ = *vp++;
					break;
				}
				case 'W': /* weather */ {
					// TODO: weather (also consider how to express future weather)
					const char* vp = "sunny";
					while (*vp) *output++ = *vp++;
					break;
				}
				default:
					*output = *fp;
					LOG_WARN(Pages, "Unknown path variable $%c", *fp);
					break; // will fail to find the file most likely
			}
		}
	}
	*output = 0;
	// TODO: improve safety?
}

/// \brief Selects current frame for raw path, by substituting path variables 
/// and resolving actual path (necessary if `Animation` struct file path
/// or directory path provided). Frame index can be modified incremented,
/// looping over max frames found (via anim/dir) if `goNextFrame` is true.
/// \param rawPath raw path (can contain vars to replace) points to BMP file
/// for still image, or `Animation` struct file or directory for dynamic
/// \param frameIndex (reference) current (valid) frame index of the animation
/// \param goNextFrame whenever the frame index should be pre-incremented 
/// and next frame image file fetched.
/// \return File for current frame (if found), should fail when cast to boolean on error
File selectCurrentFrameForFile(const char* rawPath, uint8_t& frameIndex, bool goNextFrame) {
	using namespace pages;
	char basePath[24];
	substitutePathVariables(basePath, rawPath);

	File file = LittleFS.open(basePath, "r");
	if (!file) {
		LOG_DEBUG(Pages, "Failed to open base path '%s'", basePath);
		return file;
	}
	if (file.isFile()) {
		uint16_t signature;
		file.read(reinterpret_cast<uint8_t*>(&signature), sizeof(signature));
		switch (signature) {
			case BMP::expectedSignature:
				// Return the found BMP directly, effectively there is no other frames,
				// even if there are other (even numbered) BMP files in the same directory.
				return file;
			case Animation::expectedSignature: {
				Animation animation;
				file.read(reinterpret_cast<uint8_t*>(&animation), sizeof(animation));
				
				if (goNextFrame) {
					while (animation.frames[frameIndex].path[0]) {
						if (frameIndex++ >= Animation::maxFrames) {
							frameIndex = 0;
							break;
						}
					}
				}

				const char* actualPath = animation.frames[frameIndex].path;

				file.close();
				file = LittleFS.open(actualPath, "r");
				if (!file) {
					LOG_DEBUG(Pages, "Failed to open '%s'", actualPath);
				}
				return file;
			}
			default:
				LOG_ERROR(Pages, "Invalid signature");
				return File(); // so it evaluates to false on boolean operator
		}
	}
	else /* directory */ {
		char actualPath[32];
		snprintf(
			actualPath, sizeof(actualPath), "%s/%u.bmp", 
			basePath,
			frameIndex + (goNextFrame ? 1 : 0)
		);

		if (LittleFS.exists(actualPath)) {
			frameIndex += (goNextFrame ? 1 : 0);
		}
		else {
			if (goNextFrame) 
				frameIndex = 0;
			snprintf(
				actualPath, sizeof(actualPath), "%s/0.bmp", 
				basePath
			);
		}
		LOG_TRACE(Pages, "frameIndex=%u", frameIndex);

		file.close();
		file = LittleFS.open(actualPath, "r");
		if (!file) {
			LOG_DEBUG(Pages, "Failed to open '%s'", actualPath);
		}

		return file;
	}
}

void updatePagesStuff() {
	using namespace pages;
	millis_t currentMillis = millis();

	// Going to next pages
	if (activePage.hasNextPage()) {
		const auto durationFromLast = static_cast<uint16_t>(currentMillis - lastPageChange);
		if (durationFromLast >= activePage.duration) {
			changeActivePage(activePage.next);
		}
	}

	// Background
	bool goNextBackground = false;
	if (activePage.backgroundDuration != 0) {
		const auto durationFromLast = static_cast<uint16_t>(currentMillis - lastBackgroundFrame);
		if (durationFromLast >= activePage.backgroundDuration) {
			lastBackgroundFrame = currentMillis;
			goNextBackground = true;
		}
	}
	if (not activePage.usesBackgroundFromFile()) {
		display.fillScreen(activePage.backgroundColors.primary);
	}
	else /* image(s) used */ {
		LOG_TRACE(Pages, "Background:");
		File file = selectCurrentFrameForFile(
			activePage.backgroundPath, 
			backgroundFrameIndex,
			goNextBackground
		);
		if (file) {
			BMP::drawToDisplay(file, 0, 0);
			file.close();
		}
		else {
			// TODO: error once?
			LOG_ERROR(Pages, "Failed to select frame");
		}
	}

	// Sprites
	for (uint8_t i = 0; i < Page::maxSprites; i++) {
		const auto& sprite = activePage.sprites[i];
		if (sprite.common.type == Sprite::Type::None) {
			continue;
		}
		LOG_TRACE(Pages, "Sprite %u. type=%u x=%u y=%u", i, sprite.common.type, sprite.common.x, sprite.common.y);

		// TODO: maybe rewrite it more object oriented way? 
		//  Having sprites/background class have display method.
		switch (sprite.common.type) {
			case Sprite::Type::None:
				// Skip not used sprites
				break;
			case Sprite::Type::Text:
				display.setFont(fontById(sprite.text.font));
				display.setTextColor(sprite.text.color);
				display.setCursor(sprite.common.x, sprite.common.y);
				display.print(sprite.text.text);
				// TODO: dot size
				break;
			case Sprite::Type::Time: {
				display.setFont(fontById(sprite.time.font));
				display.setTextColor(sprite.time.color);
				display.setCursor(sprite.common.x, sprite.common.y);

				char buffer[32];
				std::time_t time = std::time({});
				std::tm* tm = sprite.time.useUTC ? std::gmtime(&time) : std::localtime(&time);
				std::strftime(buffer, sizeof(buffer), sprite.time.format, tm);
				
				display.print(buffer);
				// TODO: dot size, colon fix, blinking colons
				break;
			}
			case Sprite::Type::Temperature: {
				display.setFont(fontById(sprite.temperature.font));
				display.setCursor(sprite.common.x, sprite.common.y);

				char buffer[16];
				sprintf(buffer, "%*.f", sprite.temperature.precision, temperature);
				// TODO: other temperature sources
				// TODO: ...

				display.print(buffer);
				// TODO: dot size, degree size, blinking colons
				// TODO: degree size

				break;
			}
			case Sprite::Type::Image: 
			case Sprite::Type::Animation: {
				bool goNextFrame = false;
				if (sprite.file.frameDuration != 0) {
					const auto durationFromLast = static_cast<uint16_t>(currentMillis - lastSpriteFrame[i]);
					if (durationFromLast >= sprite.file.frameDuration) {
						lastSpriteFrame[i] = currentMillis;
						goNextFrame = true;
					}
				}
				// TODO: use frame duration from animation file if present

				File file = selectCurrentFrameForFile(
					activePage.backgroundPath, 
					spriteFrameIndex[i],
					goNextFrame
				);
				if (file) {
					BMP::drawToDisplay(file, sprite.common.x, sprite.common.y, sprite.file.transparentColor);
					file.close();
				}
				else {
					// TODO: error once?
					LOG_ERROR(Pages, "Failed to select frame");
				}

				break;
			}
			case Sprite::Type::CustomChar: 
				const auto xLimit = sprite.common.x + sprite.customChar.width;
				const auto yLimit = sprite.common.y + sprite.customChar.height();
				uint8_t i = 0;
				uint8_t mask = 1;
				display.startWrite();
				for (uint8_t y = sprite.common.y; y < yLimit; y++) {
					for (uint8_t x = sprite.common.x; x < xLimit; x++) {
						if (sprite.customChar.data[i] & mask) {
							display.writePixel(x, y, sprite.customChar.color);
						}
						mask <<= 1;
						if (!mask) {
							i += 1;
							mask = 1;
						}
					}
				}
				display.endWrite();
				break;
		}
	}

	// Analog clock
	{
		// TODO: analog clock
	}
}

////////////////////////////////////////////////////////////////////////////////

void setup() {
	// Initialize Serial console
	Serial.begin(115200);
	Serial.println("\033[2J\nHello!"); // clears serial output garbage
	delay(1000);

	// Initialize display 
	display.begin(8);
	display.setFastUpdate(true);
	displayTicker.attach_ms(DISPLAY_INTERVAL, [] {
		display.display(DISPLAY_SHOW_TIME);
	});
	display.clearDisplay();
	display.setBrightness(127);

#if TEST_COLORS_ON_START
	display.fillScreen(display.color565(255, 0, 0));
	delay(2000);
	display.fillScreen(display.color565(0, 255, 0));
	delay(2000);
	display.fillScreen(display.color565(0, 0, 255));
	delay(2000);

	display.fillScreen(display.color565(255, 255, 255));
	delay(5000);
#endif

	// Initialize EEPROM
	{
		EEPROM.begin(sizeof(Settings));
		delay(100);
		settings = reinterpret_cast<Settings*>(EEPROM.getDataPtr());

		if (settings->calculateChecksum() != settings->checksum) {
			LOG_INFO(EEPROM, "Checksum miss-match, reseting settings to default.");
			settings->resetToDefault();
			Network::resetConfig();
			settings->prepareForSave();

			EEPROM.commit();
			// TODO: show info on display about reseting EEPROM
			delay(3000);
			ESP.restart();
		}
	}

	// Initialize networking
	Network::setup();
	if (settings->network.mode == Settings::Network::DISABLED) {
		showIP = false;
	}

	// Initialize thermometer(s)
	oneWire.begin(D3);
	oneWireThermometers.begin();
	oneWireThermometers.requestTemperatures();
	float t = oneWireThermometers.getTempCByIndex(0);
	if (t == DEVICE_DISCONNECTED_C) {
		LOG_ERROR(Temperature, "Not connected");
	}
	else {
		temperature = t;
		LOG_DEBUG(Temperature, "First read: %.1f", temperature);
	}
	oneWireThermometers.setWaitForConversion(true);
	oneWireThermometers.requestTemperatures();

	// Initialize NTP
	NTP::setup();

	// Initialize file system
	// LittleFS.setConfig(LittleFSConfig(/*autoFormat=*/ false));
	LittleFS.begin();

	// Initialize pages system
	prepareDefaultActivePage();
	changeActivePage(0);

	// Register server handlers
	webServer.on(F("/"), []() {
		WEB_USE_CACHE_STATIC(webServer);
		WEB_USE_GZIP_STATIC(webServer);
		webServer.send(200, WEB_CONTENT_TYPE_TEXT_HTML, WEB_index_html_CONTENT, WEB_index_html_CONTENT_LENGTH);
	});
	WEB_REGISTER_ALL_STATIC(webServer);

	webServer.on(F("/status"), []() {
		char timeString[24];
		std::time_t time = std::time({});
		std::strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time));
		
		constexpr unsigned int bufferLength = 256;
		char buffer[bufferLength];
		int ret = snprintf(
			buffer, bufferLength,
			"{"
				"\"temperature\":%.2f,"
				"\"timestamp\":\"%s\","
				"\"rssi\":%d"
			"}",
			temperature,
			timeString,
			WiFi.RSSI()
		); // not `snprintf_P` for better performance
		if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		else {
			webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
		}
	});

	webServer.on(F("/config"), []() {
		if constexpr (debugLevel >= LEVEL_DEBUG) {
			// Allow changing time for testing
			if (const String& str = webServer.arg("timestamp"); !str.isEmpty()) {
				// Format should be like: "2004-02-12T15:19:21" (without time zones)
				const char* cstr = str.c_str();
				tm tm {
					.tm_sec  = atoi(cstr + 17),
					.tm_min  = atoi(cstr + 14),
					.tm_hour = atoi(cstr + 11),
					.tm_mday = atoi(cstr + 8),
					.tm_mon  = atoi(cstr + 5),
					.tm_year = atoi(cstr + 0)
				};
				std::time_t time = std::mktime(&tm);
				timeval tv {
					.tv_sec = time,
					.tv_usec = 500'000,
				};
				settimeofday(&tv, nullptr);
			}
		}

		// Handle network config
		Network::handleConfigArgs();

		// Save to EEPROm
		if (parseBoolean(webServer.arg("save").c_str())) {
			LOG_DEBUG(EEPROM, "Preparing to save EEPROM");
			LOG_TRACE(EEPROM, "settings ptr = %p", settings);
			LOG_TRACE(EEPROM, "current checksum    = %u", settings->checksum);
			LOG_TRACE(EEPROM, "calculated checksum = %u", settings->calculateChecksum());
			if (settings->prepareForSave()) {
				EEPROM.commit();
				LOG_DEBUG(EEPROM, "EEPROM saved");
			}
		}

		// Response with current config
		constexpr unsigned int bufferLength = 640;
		char buffer[bufferLength];
		int ret = snprintf_P(
			buffer, bufferLength,
			PSTR("{"
				"\"network\":%s"
			"}"),
			Network::getConfigJSON().get()
		);
		if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		else {
			webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
		}

		// As somebody connected, remove flag
		showIP = false;
	});

	if constexpr (debugLevel >= LEVEL_DEBUG) {
		// Hidden API for testing proposes
		webServer.on(F("/test"), []() {
			webServer.send(204);
		});
	}

	webServer.addHandler(&pages::requestHandler);

	webServer.onNotFound([]() {
		webServer.send(404, WEB_CONTENT_TYPE_TEXT_PLAIN, PSTR("Not found\n\n"));
	});
	webServer.begin();
}

////////////////////////////////////////////////////////////////////////////////

#define UPDATE_EVERY(interval)          \
	for(                                \
		static millis_t prev = 0;       \
		currentMillis - prev > interval;\
		currentMillis = prev = millis() \
	)

void loop() {
	millis_t currentMillis = millis();

	webServer.handleClient();

	// TODO: show IP on display for a while or until connected

	UPDATE_EVERY(60 * 60 * 1000) {
		NTP::update();
	}

	// Update thermometer
	if (oneWireThermometers.isConversionComplete()) {
		// Update thermometer read
		float t = oneWireThermometers.getTempCByIndex(0);
		if (t != DEVICE_DISCONNECTED_C) {
			temperature = (temperature + t) / 2;
		}

		oneWireThermometers.requestTemperatures();
	}

	updatePagesStuff();
}
