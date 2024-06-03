#include <Adafruit_GFX.h>
#include <SPI.h>

namespace {

constexpr unsigned floor_log2(unsigned x)
{
	return x == 1 ? 0 : 1 + floor_log2(x >> 1);
}

}

template <
	int8_t PIN_LATCH = 16,
	int8_t PIN_OE = 2,
	int8_t PIN_A = 5,
	int8_t PIN_B = 4,
	int8_t PIN_C = 15,
	int8_t PIN_D = 12,
	int8_t PIN_E = -1,
	uint8_t constWidth = 64,
	uint8_t constHeight = 32,
	uint8_t rowPattern = 8,
	uint8_t colorDepth = 4,
	uint32_t spiFrequency = 20000000
>
class MyPxMatrix : public Adafruit_GFX
{
	static_assert(rowPattern <  2 || PIN_A >= 0);
	static_assert(rowPattern <  4 || PIN_B >= 0);
	static_assert(rowPattern <  8 || PIN_C >= 0);
	static_assert(rowPattern < 16 || PIN_D >= 0);
	static_assert(rowPattern < 32 || PIN_E >= 0);

	static_assert(0 < colorDepth && colorDepth <= 5);

	static constexpr size_t panelsWidth = 1;
	static constexpr size_t panelWidthBytes = constWidth / panelsWidth / 8;
	static constexpr size_t patternColorBytes = constHeight / rowPattern * constWidth / 8;
	static constexpr size_t sendBufferSize = patternColorBytes * 3;
	static constexpr size_t noDepthBufferSize = constWidth * constHeight * 3 / 8;
	static constexpr size_t rowPatternBits = floor_log2(rowPattern);

	////////////////////////////////////////
	// Fields

	alignas(uint32_t)
	uint8_t buffer[noDepthBufferSize * colorDepth];
	// uint32_t* rowsPointers[height];

public:
	bool flipX = false;

private:
	uint8_t displayColorDepth = 0;
	uint8_t displayRowPattern = 0;
	const uint8_t* displayNextBufferPosition = buffer + sendBufferSize;

	
	////////////////////////////////////////
	// Constructor & begin
public:
	inline MyPxMatrix() 
		: Adafruit_GFX(constWidth, constHeight) 
	{
		displayNextBufferPosition = buffer;
		// for (size_t y = 0; y < height; y++) {
		// 	rowsPointers[y] = 
		// 		sendBufferSize - 1
		// 		- panelWidthBytes * (y >> floor_log2(rowPattern));
		// 		+ (y % rowPattern) * sendBufferSize;
		// }
	}

	void begin()
	{
		SPI.begin();
		SPI.setFrequency(spiFrequency);
		SPI.setDataMode(SPI_MODE0);
		SPI.setBitOrder(MSBFIRST);

		pinMode(PIN_LATCH, OUTPUT);
		pinMode(PIN_OE, OUTPUT);
		digitalWrite(PIN_LATCH, LOW);
		digitalWrite(PIN_OE, LOW);

		if (PIN_A != -1) pinMode(PIN_A, OUTPUT);
		if (PIN_B != -1) pinMode(PIN_B, OUTPUT);
		if (PIN_C != -1) pinMode(PIN_C, OUTPUT);
		if (PIN_D != -1) pinMode(PIN_D, OUTPUT);
		if (PIN_E != -1) pinMode(PIN_E, OUTPUT);
	}

	////////////////////////////////////////
	// Drawing overrides

	virtual void drawPixel(int16_t x, int16_t y, uint16_t color) override
	{
		if (x < 0 || x >= constWidth || y < 0 || y >= constHeight)
			return;

		// TODO: rotate?

		// Allow for flipping in
		if (!flipX) 
			x = constWidth - 1 - x;

		const auto xByte = x / 8;
		const auto xBit  = x % 8;

		const uint_fast32_t rOffset = (y % rowPattern) * sendBufferSize 
			+ (sendBufferSize - 1) - xByte - panelWidthBytes * (y >> floor_log2(rowPattern));
		const uint_fast32_t gOffset = rOffset - patternColorBytes;
		const uint_fast32_t bOffset = gOffset - patternColorBytes;

		// Convert RGB565 to components with 5 bit precision; only 5 LSB used
		const uint_fast8_t r = color >> 11;
		const uint_fast8_t g = color >> 6; // 6 instead 5, ignoring 6th bit
		const uint_fast8_t b = color;

		#pragma GCC unroll 4
		for (uint_fast8_t i = 0; i < colorDepth; i++) {
			const size_t depthBufferOffset = noDepthBufferSize * i;

			if ((r >> i) & 1)
				buffer[depthBufferOffset + rOffset] |= 1 << xBit;
			else
				buffer[depthBufferOffset + rOffset] &= ~(1 << xBit);

			if ((g >> i) & 1)
				buffer[depthBufferOffset + gOffset] |= 1 << xBit;
			else
				buffer[depthBufferOffset + gOffset] &= ~(1 << xBit);

			if ((b >> i) & 1)
				buffer[depthBufferOffset + bOffset] |= 1 << xBit;
			else
				buffer[depthBufferOffset + bOffset] &= ~(1 << xBit);
		}
	}

	////////////////////////////////////////
	// Display driving

	/// Updates the display by minimal step (single minimal chunk).
	/// The `minimalShowTime` is in microseconds.
	void displayStep(uint8_t minimalShowTime)
	{
		setMux(displayRowPattern);
		pulseLatch();
		enableOutput();
		unsigned long start = micros();
		SPI.writeBytes(displayNextBufferPosition, sendBufferSize);

		displayNextBufferPosition += sendBufferSize;
		displayRowPattern += 1;
		if (displayRowPattern >= rowPattern) {
			displayRowPattern = 0;
			displayColorDepth += 1;
			if (displayColorDepth >= colorDepth) {
				displayColorDepth = 0;
				displayNextBufferPosition = buffer + sendBufferSize;
			}
		}

		unsigned long expected = minimalShowTime * (1 << displayColorDepth);
		while (micros() - start < expected) {
			asm volatile ("nop");
		}
		disableOutput();
		// Serial.printf("d rP=%u cD=%u bP=%u\n", 
		// 	displayRowPattern, displayColorDepth, displayNextBufferPosition - buffer);
	}

private:
#ifdef ESP8266
	static constexpr uint16_t prepareMaskForGPIO()
	{
		uint16_t mask = 0;
		if constexpr (PIN_A != -1) mask |= (1 << PIN_A);
		if constexpr (PIN_B != -1) mask |= (1 << PIN_B);
		if constexpr (PIN_C != -1) mask |= (1 << PIN_C);
		if constexpr (PIN_D != -1) mask |= (1 << PIN_D);
		if constexpr (PIN_E != -1) mask |= (1 << PIN_E);
		return mask;
	}

	inline void setMux(uint8_t row)
	{
		// Faster I/O for ESP8266, but no support for GPIO16.
		constexpr uint16_t mask = prepareMaskForGPIO();
		uint16_t value = 0;
		if constexpr (PIN_A != -1) value |= (!!(row &  1) << PIN_A);
		if constexpr (PIN_B != -1) value |= (!!(row &  2) << PIN_B);
		if constexpr (PIN_C != -1) value |= (!!(row &  4) << PIN_C);
		if constexpr (PIN_D != -1) value |= (!!(row &  8) << PIN_D);
		if constexpr (PIN_E != -1) value |= (!!(row & 16) << PIN_E);
		GPOS = value & mask;
		GPOC = ~value & mask;
	}
#else
	inline void setMux(uint8_t row)
	{
		// Fallback to Arduino ways
		digitalWrite(PIN_A, row & 1);
		digitalWrite(PIN_B, row & 2);
		if (PIN_C != -1) digitalWrite(PIN_C, row & 4);
		if (PIN_D != -1) digitalWrite(PIN_D, row & 8);
		if (PIN_E != -1) digitalWrite(PIN_E, row & 16);
	}
#endif

	inline void pulseLatch()
	{
#ifdef ESP8266
		if constexpr (PIN_LATCH == 16) {
			GP16O = GP16O | 1;
			GP16O = GP16O & ~1;
		}
		else {
			GPOS = 1 << PIN_LATCH;
			GPOC = ~(1 << PIN_LATCH);
		}
#else
		digitalWrite(PIN_LATCH, HIGH);
		digitalWrite(PIN_LATCH, LOW);
#endif
	}

	inline void enableOutput()
	{
#ifdef ESP8266
		if constexpr (PIN_LATCH == 16) {
			GP16O = GP16O & ~1;
		}
		else {
			GPOC = 1 << PIN_OE;
		}
#else
		digitalWrite(PIN_OE, LOW);
#endif
	}

	inline void disableOutput()
	{
#ifdef ESP8266
		if constexpr (PIN_LATCH == 16) {
			GP16O = GP16O | 1;

		}
		else {
			GPOS = 1 << PIN_OE;
		}
#else
		digitalWrite(PIN_OE, HIGH);
#endif
	}
};
