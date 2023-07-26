#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

/**
 * Simple SNTP client.
 * See https://datatracker.ietf.org/doc/rfc5905/ for the protocol details.
 */
class NTPClient {
public:
	static constexpr uint16_t defaultPoolServerPort = 123;
	static const char* const defaultPoolServerName;
	static const uint32_t offsetFrom1900 = 2208988800;

protected:
	UDP& udp;

	union {
		ip4_addr_t poolServerIP;
		const char* poolServerName;
		static_assert(sizeof(poolServerName) == sizeof(poolServerIP));
	};

	bool poolServerSpecifiedByName;

public:
	ip4_addr_t getPoolServerIP() const;
	const char* getPoolServerName() const;
	void setPoolServerIP(const ip4_addr_t ip);
	void setPoolServerName(const char* name);

	/**
	 * Returns number of minutes offset used, relative to UTC time.
	 */
	int16_t getTimeOffset() const;
	/**
	 * Set returns number of minutes offset used, relative to UTC time.
	 */
	void setTimeOffset(int16_t minutes);

	/**
	 * Last update `millis()` timestamp.
	 */
	uint32_t lastUpdateMillis;

	/**
	 * Seconds part of latest response got from the NTP server, with configured time offset applied.
	 */
	uint32_t lastResponseSeconds;
	/**
	 * Milliseconds converted from fraction part of latest response got from the NTP server.
	 */
	uint16_t lastResponseMillis;

	NTPClient(UDP& udp)
		: udp(udp), poolServerName(defaultPoolServerName), poolServerSpecifiedByName(true)
	{}

	NTPClient(UDP& udp, const char* poolServerName)
		: udp(udp), poolServerName(poolServerName), poolServerSpecifiedByName(true)
	{}

	NTPClient(UDP& udp, ip4_addr_t poolServerIP)
		: udp(udp), poolServerIP(poolServerIP), poolServerSpecifiedByName(false)
	{}

protected:
	static constexpr size_t ntpPacketSize = 48;

	/**
	 * Sends NTP packet to the server.
	 * 
	 * Returns false is something went wrong.
	 */
	bool sendNTPPacket();

public:
	/**
	 * Updates the time hold by NTP client. 
	 * 
	 * Returns false is something went wrong, like timeout.
	 */
	bool update(unsigned long timeout = 500);

	/**
	 * Return total milliseconds since last update for given `millis()` timestamp.
	 * 
	 * Overflow can happen if not updated at least once per like 40 days.
	 */
	uint32_t millisSinceUpdate(const uint32_t currentMillis) const;

public:
	/**
	 * Return time as number of seconds since Unix epoch (Jan 1 1970) for given `millis()` timestamp.
	 */
	uint32_t unixSeconds(const uint32_t currentMillis) const;
	/**
	 * Return current time as number of seconds since Unix epoch (Jan 1 1970).
	 */
	uint32_t unixSeconds() const;

	/**
	 * Return time as number of milliseconds since Unix epoch (Jan 1 1970) for given `millis()` timestamp.
	 */
	uint64_t unixMillis(const uint32_t currentMillis) const;
	/**
	 * Return time as number of milliseconds since Unix epoch (Jan 1 1970).
	 */
	uint64_t unixMillis() const;
};
