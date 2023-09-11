#pragma once // should be included only inside common.hpp!

////////////////////////////////////////////////////////////////////////////////
// Main configuration

#define DEBUG LEVEL_DEBUG
constexpr auto debugLevel = DEBUG;

USE_LOG_LEVEL_DEFAULT(LEVEL_DEBUG);
USE_LOG_LEVEL(Network,          LEVEL_INFO);
USE_LOG_LEVEL(EEPROM,           LEVEL_INFO);
USE_LOG_LEVEL(Web,              LEVEL_INFO);
USE_LOG_LEVEL(Time,             LEVEL_INFO);
USE_LOG_LEVEL(Temperature,      LEVEL_INFO);
USE_LOG_LEVEL(Pages,            LEVEL_TRACE);
USE_LOG_LEVEL(BMP,              LEVEL_TRACE);

/// Timeouts for networking
constexpr unsigned long timeoutForConnectingWiFi = 5000; // ms

////////////////////////////////////////////////////////////////////////////////
// Settings structure (persisted in EEPROM)

// Settings structure (in EEPROM)
struct Settings {
	////////////////////////////////////////
	// 0x000 - 0x020: Checksum

	uint8_t _emptyBeginPad[28];
	uint32_t checksum;

	uint32_t calculateChecksum() {
		constexpr uint16_t prefixLength = offsetof(Settings, checksum) + sizeof(checksum);
		return crc32(reinterpret_cast<uint8_t*>(this) + prefixLength, sizeof(Settings) - prefixLength);
	}

	bool prepareForSave() {
		uint32_t calculatedChecksum = calculateChecksum();
		bool changed = checksum != calculatedChecksum;
		checksum = calculatedChecksum;
		return changed;
	}

	////////////////////////////////////////

	uint8_t _beforeNetworkPad[0x100 - 0x020];
	
	////////////////////////////////////////
	// 0x100 - 0x160: Some network and cloud settings.

	// Some network settings are persisted by internal SDK.
	struct Network {
		enum Mode : uint8_t {
			DISABLED = 0b00,
			STATION  = 0b01,
			AP       = 0b10,
			FALLBACK = 0b11,
		};

		union {
			struct {
				Mode mode          : 2;
				bool staticIP      : 1;
			};
			uint8_t flags;
		};
		ip_info ipInfo;
		ip4_addr_t dns1;
		ip4_addr_t dns2;

		char _pad[8];
		char basicAuthEncoded[64] = "";

		inline bool usesStation() { return mode & 0b01; }
	} network;
	static_assert(0x04 == offsetof(Network, ipInfo));
	static_assert(0x0C == sizeof(ip_info));
	static_assert(0x10 == offsetof(Network, dns1));
	static_assert(0x14 == offsetof(Network, dns2));
	static_assert(0x20 == offsetof(Network, basicAuthEncoded));
	static_assert(sizeof(network) == 0x60);

	////////////////////////////////////////
	// 0x160 - 0x200: Cloud settings.

	struct {
		char secret[16] = "";
		uint32_t interval = 60000;
		char _pad[12];
		char endpointURL[128];
	} cloud;
	static_assert(sizeof(cloud) == 0x0A0);





	////////////////////////////////////////

	void resetToDefault() {
		new (this) Settings(); // will apply all defaults
		network.mode = Network::Mode::AP;
	}
};
static_assert(0x100 == offsetof(Settings, network));
static_assert(0x160 == offsetof(Settings, cloud));
static_assert(sizeof(Settings) == 0x200);
static_assert(0x000 + 28 == offsetof(Settings, checksum));
