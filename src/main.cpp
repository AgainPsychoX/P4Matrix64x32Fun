#include "common.hpp"
#define PxMATRIX_double_buffer true
#include <PxMatrix.h>
#include <Fonts/FreeSerifBold12pt7b.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <NTPClient.hpp>
#include <TimeUtils.hpp>
#include <ctime>

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
			.tv_usec = remainingMillis % 1000,
		};
		settimeofday(&tv, nullptr);
	}
	else {
		LOG_WARN(Time, "Failed to update time from NTP");
	}
}

inline IPAddress stringIPAddress(const char* str) {
	IPAddress a;
	a.fromString(str);
	return a;
}

void setup() {
	// Initialize Serial console
	Serial.begin(115200);
	Serial.println();

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

	sprintf(buffer, "%lu", currentMillis);
	display.fillRect(0, 24, 64, 8, display.color565(0, 0, 0));
	display.setCursor(0, 32 - 1);
	display.setFont(nullptr); // back to built-in 6x8
	display.setTextColor(display.color565(0, 0, 63));
	display.print(buffer);

	delay(50);
}
