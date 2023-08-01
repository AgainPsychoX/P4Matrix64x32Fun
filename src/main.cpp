#include "common.hpp"
#define PxMATRIX_double_buffer false
#include <PxMatrix.h>
#include <Fonts/FreeSerifBold12pt7b.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <vector>
#include <tuple>
#include <DallasTemperature.h> // for DS18B20 thermometer
#include "Network.hpp"
#include "NTP.hpp"
#include "colors.hpp"
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

////////////////////////////////////////////////////////////////////////////////

Settings* settings;
ESP8266WebServer webServer(80);
bool showIP = true;
constexpr unsigned int showIPtimeout = 20000;

////////////////////////////////////////////////////////////////////////////////

OneWire oneWire;
DallasTemperature oneWireThermometers(&oneWire);
float temperature = 0; // avg of last and current read (simplest noise reduction)

/// \brief Holds expected colors for given temperatures. 
///        Should be always kept sorted by temperature (ascending).
std::vector<std::tuple<float, colors::RGB>> keyColorsForTemperatures = {
	{5.0f,  {0, 0, 255}},
	{15.0f, {0, 255, 0}},
	{25.0f, {255, 0, 0}},
};

/// \brief Finds or interpolates (in HSL space) color for temperature, 
///        based on `keyColorsForTemperatures` (which is expected to be sorted).
/// \param temperature temperature to get color for
/// \return RGB color
colors::RGB colorForTemperature(float temperature) {
	using namespace colors;
	
	auto const& [firstTemperature, firstColor] = keyColorsForTemperatures.front();
	if (temperature <= firstTemperature) {
		return firstColor;
	}

	for (
		auto it = keyColorsForTemperatures.cbegin() + 1;
		it != keyColorsForTemperatures.cend();
		++it
	) {
		auto const& [previousTemperature, previousColor] = *(it - 1);
		auto const& [currentTemperature, currentColor] = *it;

		if (temperature <= currentTemperature) {
			float ratio = (temperature - previousTemperature) / (currentTemperature - previousTemperature);
			return toRGB(interpolateHSL(toHSL(previousColor), toHSL(currentColor), ratio));
		}
	}

	auto const& [lastTemperature, lastColor] = keyColorsForTemperatures.back();
	return lastColor;
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
		);
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

	webServer.on(F("/saveEEPROM"), []() {
		LOG_DEBUG(EEPROM, "Preparing to save EEPROM");
		LOG_TRACE(EEPROM, "settings ptr = %p", settings);
		LOG_TRACE(EEPROM, "current checksum    = %u", settings->checksum);
		LOG_TRACE(EEPROM, "calculated checksum = %u", settings->calculateChecksum());
		if (settings->prepareForSave()) {
			EEPROM.commit();
			LOG_DEBUG(EEPROM, "EEPROM saved");
		}
		webServer.send(200);
	});

	if constexpr (debugLevel >= LEVEL_DEBUG) {
		// Hidden API for testing proposes
		webServer.on(F("/test"), []() {
			webServer.send(204);
		});
	}

	webServer.onNotFound([]() {
		webServer.send(404, WEB_CONTENT_TYPE_TEXT_PLAIN, PSTR("Not found\n\n"));
	});
	webServer.begin();
}

////////////////////////////////////////////////////////////////////////////////

#define UPDATE_EVERY(interval)          \
	for(                                \
		static unsigned long prev = 0;  \
		currentMillis - prev > interval;\
		currentMillis = prev = millis() \
	)

void loop() {
	unsigned long currentMillis = millis();

	webServer.handleClient();

	// TODO: show IP on display for a while or until connected

	UPDATE_EVERY(60 * 60 * 1000) {
		NTP::update();
	}

	// Update and display thermometer
	if (oneWireThermometers.isConversionComplete()) {
		// Update thermometer read
		float t = oneWireThermometers.getTempCByIndex(0);
		if (t != DEVICE_DISCONNECTED_C) {
			temperature = (temperature + t) / 2;
		}

		oneWireThermometers.requestTemperatures();

		// Display temperature 
		{
			char buffer[16];
			display.fillRect(0, 24, 64, 8, display.color565(0, 0, 0));
			display.setFont(nullptr); // back to built-in 6x8
			uint16_t color = colors::to565(colorForTemperature(temperature));
			display.setTextColor(color);
			sprintf(buffer, "%.1f", temperature);
			size_t i = 1;
			while (buffer[i++] != '.'); // find char after dot
			buffer[i - 1] = 0;
			display.setCursor(0, 25);
			display.print(buffer);
			display.fillRect(display.getCursorX(), 25 + 5, 2, 2, color);
			display.setCursor(display.getCursorX() + 3, 25);
			display.print(buffer + i);
			display.drawPixel(display.getCursorX(),     25 + 1, color);
			display.drawPixel(display.getCursorX() + 2, 25 + 1, color);
			display.drawPixel(display.getCursorX() + 1, 25,     color);
			display.drawPixel(display.getCursorX() + 1, 25 + 2, color);
			display.setCursor(display.getCursorX() + 4, 25);
			display.print('C');
		}
	}

	// Display digital clock
	UPDATE_EVERY(1000) {
		char buffer[16];
		std::time_t time = std::time({});
		std::strftime(std::data(buffer), std::size(buffer), "%T", std::localtime(&time));
		buffer[2] = 0;
		buffer[5] = 0;

		display.setFont(&FreeSerifBold12pt7b);
		display.setTextColor(display.color565(0, 0, 255));
		display.fillRect(0, 0, 64, 24, display.color565(0, 0, 0));

		display.setCursor(0, 0 + 15); 
		display.print(buffer + 0); // hours

		if (currentMillis % 2000 > 1000) {
			display.setCursor(23, 0 + 15); 
			display.print(':');
		}

		display.setCursor(30, 0 + 15); 
		display.print(buffer + 3); // minutes
		// display.print(buffer + 6); // seconds
	}
}
