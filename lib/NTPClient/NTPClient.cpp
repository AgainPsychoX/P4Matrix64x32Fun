#include "NTPClient.hpp"

const char* const NTPClient::defaultPoolServerName = "europe.pool.ntp.org";

ip4_addr_t NTPClient::getPoolServerIP() const {
	return poolServerSpecifiedByName ? ip4_addr_t{0} : poolServerIP;
}
const char* NTPClient::getPoolServerName() const {
	return poolServerSpecifiedByName ? poolServerName : nullptr;
}

void NTPClient::setPoolServerIP(const ip4_addr_t ip) {
	poolServerIP = ip;
	poolServerSpecifiedByName = false;
}
void NTPClient::setPoolServerName(const char* name) {
	poolServerName = name;
	poolServerSpecifiedByName = true;
}

bool NTPClient::sendNTPPacket() {
	byte buffer[ntpPacketSize];
	memset(buffer, 0, ntpPacketSize);
	buffer[0] = 0b00100011; // LI, Version, Mode

	if (poolServerSpecifiedByName) {
		if (udp.beginPacket(this->poolServerName, defaultPoolServerPort) != 1)
			return false;
	} 
	else {
		if (udp.beginPacket(this->poolServerIP, defaultPoolServerPort) != 1) 
			return false;
	}

	if (udp.write(buffer, ntpPacketSize) < ntpPacketSize) 
		return false;

	if (udp.endPacket() != 1)
		return false;

	return true;
}

bool NTPClient::update(unsigned long timeout) {
	// Flush any existing packets
	while (udp.parsePacket() != 0) {
		udp.flush();
	}

	if (!sendNTPPacket())
		return false;

	// Wait for response
	int packetSize;
	{
		uint32_t start = millis();
		do {
			delay(4);
			packetSize = udp.parsePacket();
			if (millis() - start > timeout) {
				return false;
			}
		}
		while (packetSize == 0);

		lastUpdateMillis = millis();
	}

	byte buffer[ntpPacketSize];
	udp.read(buffer, ntpPacketSize);

	// NTP returns time since Jan 1 1900 in seconds and fraction parts, each 32 bits.
	const uint32_t seconds = ntohl(*reinterpret_cast<const uint32_t*>(buffer + 40));
	lastResponseSeconds = seconds - offsetFrom1900;
	const uint64_t fraction = ntohl(*reinterpret_cast<const uint16_t*>(buffer + 44));
	lastResponseMillis = (fraction * 1000) >> 32;

	return true;
}

uint32_t NTPClient::millisSinceUpdate(const uint32_t currentMillis) const {
	return currentMillis < lastUpdateMillis
		? (std::numeric_limits<uint32_t>::max() - lastUpdateMillis + currentMillis)
		: currentMillis - lastUpdateMillis
	;
}

uint32_t NTPClient::unixSeconds(const uint32_t currentMillis) const {
	return lastResponseSeconds + (static_cast<uint32_t>(lastResponseMillis) + millisSinceUpdate(currentMillis)) / 1000;
}
uint32_t NTPClient::unixSeconds() const {
	return unixSeconds(millis());
}

uint64_t NTPClient::unixMillis(const uint32_t currentMillis) const {
	return static_cast<uint64_t>(lastResponseSeconds) * 1000 + lastResponseMillis + millisSinceUpdate(currentMillis);
}
uint64_t NTPClient::unixMillis() const {
	return unixMillis(millis());
}
