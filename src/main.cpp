#include "common.hpp"
#define PxMATRIX_double_buffer false
#include <PxMatrix.h>
#include <Fonts/FreeSerifBold12pt7b.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <NTPClient.hpp>
#include <TimeUtils.hpp>
#include <ctime>
#include <vector>
#include <tuple>
#include <DallasTemperature.h> // for DS18B20 thermometer
#include "colors.hpp"

////////////////////////////////////////////////////////////////////////////////

#ifndef WIFI_SSID
#	warning Using default WiFi settings
#	define WIFI_MODE WIFI_AP
#	define WIFI_SSID "MatrixDisplay"
#	define WIFI_PASS "12345678"
#	define WIFI_CHANNEL 1
#endif
#ifndef WIFI_MODE
#	define WIFI_MODE WIFI_STA
#endif
#ifndef WIFI_CHANNEL
#	define WIFI_CHANNEL 0
#endif
#ifndef WIFI_IP // will use some defaults
#	define WIFI_IP NULL
#	define WIFI_NETMASK NULL
#	define WIFI_GATEWAY NULL
#endif
#ifndef WIFI_DNS
#	define WIFI_DNS "1.1.1.1"
#endif

inline IPAddress stringIPAddress(const char* str) {
	IPAddress a;
	a.fromString(str);
	return a;
}

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

/// Local UDP socket for NTP client.
WiFiUDP ntpUDP;
/// NTP client to update time. The struct is used to store time even when NTP is not available.
NTPClient ntp(ntpUDP, "pl.pool.ntp.org");

void ntpUpdate() {
	if (ntp.update(1000)) {
		const DateTime now = DateTime::fromUnixMillis(ntp.unixMillis());
		LOG_DEBUG(Time, "Time updated from NTP: %u-%02u-%02u %02u:%02u:%02u (UTC)",
			now.year, now.month, now.day, now.hour, now.minute, now.second);
		
		uint32_t remainingMillis = static_cast<uint32_t>(ntp.lastResponseMillis) + ntp.millisSinceUpdate(millis());
		timeval tv {
			.tv_sec = ntp.lastResponseSeconds + remainingMillis / 1000,
			.tv_usec = static_cast<suseconds_t>(remainingMillis % 1000),
		};
		settimeofday(&tv, nullptr);
	}
	else {
		LOG_WARN(Time, "Failed to update time from NTP");
	}
}

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

	// Initialize networking
	LOG_DEBUG(WiFi, "MODE=" STRINGIFY(WIFI_MODE) ", SSID=" WIFI_SSID ", PASS=" WIFI_PASS ".");
	if (WIFI_MODE == WIFI_AP) {
		WiFi.softAP(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
		if (WIFI_IP) {
			WiFi.softAPConfig(stringIPAddress(WIFI_IP), stringIPAddress(WIFI_GATEWAY), stringIPAddress(WIFI_NETMASK));
		}
	}
	else if (WIFI_MODE == WIFI_STA) {
		WiFi.begin(WIFI_SSID, WIFI_PASS);
		if (WIFI_IP) {
			WiFi.config(stringIPAddress(WIFI_IP), stringIPAddress(WIFI_GATEWAY), stringIPAddress(WIFI_NETMASK), stringIPAddress(WIFI_DNS));
		}
	}
	// {
	// 	ip_info info = {
	// 		.ip      = hton(parseIPv4("192.168.55.120")),
	// 		.netmask = hton(parseIPv4("255.255.255.0")),
	// 		.gw      = hton(parseIPv4("192.168.55.1")),
	// 	};
	// 	wifi_set_ip_info(STATION_IF, &info);
	// 	wifi_station_dhcpc_stop();
	// 	wifi_fpm_do_wakeup();
	// 	wifi_fpm_close();
	// 	wifi_set_opmode(WIFI_STA);
	// 	station_config conf = {};
	// 	strncpy_P(reinterpret_cast<char*>(conf.ssid), PSTR(WIFI_SSID), sizeof(conf.ssid));
	// 	strncpy_P(reinterpret_cast<char*>(conf.password), PSTR(WIFI_PASS), sizeof(conf.password));
	// 	wifi_station_set_config(&conf);
	// }

	// Wait for WiFi connection
	Serial.print("Connecting.");
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print('.');
		delay(250);
	}
	Serial.println(" connected!");
	Serial.print("IP: ");
	Serial.println(WiFi.localIP());

	// Initialize NTP
	LOG_TRACE(Time, "Opening local UDP socket for NTP");
	ntpUDP.begin(10123);
	ntpUpdate();
	// TODO: allow changing NTP server & timezone
	setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Hardcoded for Europe/Warsaw
	tzset();
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

	UPDATE_EVERY(60 * 60 * 1000) {
		ntpUpdate();
	}

	// Update thermometer read
	if (oneWireThermometers.isConversionComplete()) {
		float t = oneWireThermometers.getTempCByIndex(0);
		if (t != DEVICE_DISCONNECTED_C) {
			temperature = (temperature + t) / 2;
		}

		oneWireThermometers.requestTemperatures();
	}

	char buffer[16];

	// Display digital clock
	{
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

	// Display temperature 
	{
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

	// sprintf(buffer, "%lu", currentMillis);
	// display.fillRect(0, 24, 64, 8, display.color565(0, 0, 0));
	// display.setCursor(0, 32 - 1);
	// display.setFont(nullptr); // back to built-in 6x8
	// display.setTextColor(display.color565(0, 0, 63));
	// display.print(buffer);

	delay(50);
}
