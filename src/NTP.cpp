#include "NTP.hpp"

#include <NTPClient.hpp>

namespace NTP {
	/// Local UDP socket for NTP client.
	WiFiUDP ntpUDP;

	/// NTP client to update time. The struct is used to store time even when NTP is not available.
	NTPClient ntp(ntpUDP, "pl.pool.ntp.org");

	void setup() {
		LOG_TRACE(Time, "Opening local UDP socket for NTP");
		ntpUDP.begin(10123);
		update();
		// TODO: allow changing NTP server & timezone
		setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Hardcoded for Europe/Warsaw
		tzset();
	}

	void update() {
		if (ntp.update(1000)) {
			char timeString[24];
			std::time_t time = std::time({});
			std::strftime(timeString, sizeof(timeString), "%FT%TZ", std::gmtime(&time));
			LOG_DEBUG(Time, "Time updated from NTP: %s (UTC)", timeString);
			
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
}
