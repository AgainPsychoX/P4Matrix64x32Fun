#include <Adafruit_GFX.h>
#include <SPI.h>

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
	uint8_t constRowPattern = 8,
	uint8_t constColorDepth = 4,
	uint32_t spiFrequency = 20000000
>
class MyPxMatrix : public Adafruit_GFX
{
	static_assert(constRowPattern <  2 || PIN_A >= 0);
	static_assert(constRowPattern <  4 || PIN_B >= 0);
	static_assert(constRowPattern <  8 || PIN_C >= 0);
	static_assert(constRowPattern < 16 || PIN_D >= 0);
	static_assert(constRowPattern < 32 || PIN_E >= 0);

	static_assert(0 < constColorDepth && constColorDepth <= 5);

	static constexpr size_t panelsWidth = 1;
	static constexpr size_t panelWidthBytes = constWidth / panelsWidth / 8;
	static constexpr size_t patternColorBytes = constHeight / constRowPattern * constWidth / 8;
	static constexpr size_t sendBufferSize = patternColorBytes * 3;
	static constexpr size_t noDepthBufferSize = constWidth * constHeight * 3 / 8;

	////////////////////////////////////////
	// Fields

	alignas(uint32_t)
	uint8_t buffer[noDepthBufferSize * constColorDepth];
#ifdef DISPLAY_ROW_POINTERS_OPTIMIZATION
	uint8_t* rowsPointers[constHeight];
#endif // DISPLAY_ROW_POINTERS_OPTIMIZATION

	__attribute__((always_inline))
	inline uint8_t* getRowPointer(uint8_t y)
	{
#ifdef DISPLAY_ROW_POINTERS_OPTIMIZATION
		return rowsPointers[y];
#else
		return buffer
			+ (sendBufferSize - 1) + (y % constRowPattern) * sendBufferSize
			- panelWidthBytes * (y / constRowPattern);
#endif
	}

public:
	bool flipX = false;
	bool flipY = false;

	/// Returns color depth in bits.
	inline uint8_t colorDepth() const { return constColorDepth; }

	/// Returns row scan pattern for the display.
	inline uint8_t rowPattern() const { return constRowPattern; }

private:
	uint8_t displayColorDepth = 0;
	uint8_t displayRowPattern = constRowPattern - 1;
	const uint8_t* displayNextBufferPosition;

	////////////////////////////////////////
	// Constructor & begin
public:
	inline MyPxMatrix() 
		: Adafruit_GFX(constWidth, constHeight) 
	{
		displayNextBufferPosition = buffer;
#ifdef DISPLAY_ROW_POINTERS_OPTIMIZATION
		for (size_t y = 0; y < constHeight; y++) {
			rowsPointers[y] = buffer
				+ (sendBufferSize - 1) + (y % constRowPattern) * sendBufferSize
				- panelWidthBytes * (y / constRowPattern);
		}
#endif // DISPLAY_ROW_POINTERS_OPTIMIZATION
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

	__attribute__((flatten))
	virtual void drawPixel(int16_t x_, int16_t y_, uint16_t color) override
	{
		if (x_ < 0 || x_ >= constWidth || y_ < 0 || y_ >= constHeight)
			return;

		// Casting to unsigned and fast types really helps here a tiny bit.
		// Also, allow for flipping; note X axis is flipped by default.
		const uint_fast16_t x = flipX ? x_ : constWidth - 1 - x_;
		const uint_fast16_t y = flipY ? constHeight - 1 - y_ : y_;

		const uint_fast16_t xByte = x / 8;
		const uint_fast8_t xBit = x % 8;

		uint8_t* pointer = getRowPointer(y) - xByte;
		const auto rgb = prepareColorComponents(color);
		setByteInBuffer(pointer, rgb, 1 << xBit);
	}

	__attribute__((flatten))
	virtual void drawFastHLine(int16_t x1_, int16_t y_, int16_t w, uint16_t color) override
	{
		if (x1_ >= constWidth || y_ < 0 || y_ >= constHeight || w <= 0)
			return;
		if (x1_ < 0) {
			w += x1_;
			if (w <= 0)
				return;
			x1_ = 0;
		}
		const auto x2_ = std::min(x1_ + w - 1, constWidth - 1);

		// Casting to unsigned and fast types really helps here a tiny bit.
		// Also, allow for flipping; note X axis is flipped by default.
		const uint_fast16_t x1 = flipX ? x1_ : constWidth - 1 - x1_;
		const uint_fast16_t x2 = flipX ? x2_ : constWidth - 1 - x2_;
		const uint_fast16_t y = flipY ? constHeight - 1 - y_ : y_;

		const uint_fast16_t x1Byte = x1 / 8;
		const uint_fast16_t x2Byte = x2 / 8;

		uint8_t* basePointer = getRowPointer(y);
		const auto rgb = prepareColorComponents(color);

		const auto x1Bit = x1 % 8;
		const auto x2Bit = x2 % 8;
		const uint8_t x1Mask = 0xFF >> (7 - x1Bit);
		const uint8_t x2Mask = 0xFF << x2Bit;

		// Handle case where the line is single byte
		if (x1Byte == x2Byte) {
			uint8_t mask = x1Mask & x2Mask;
			uint8_t* pointer = basePointer - x1Byte;

			setByteInBuffer(pointer, rgb, mask);
		}
		else /* multi-byte */ {
			// Set pointer to first byte, and calculate last byte pointer.
			// Note the inverse order, because `x1Byte > x2Byte` always true.
			uint8_t* pointer = basePointer - x1Byte;
			uint8_t* last = basePointer - x2Byte;

			// First byte
			setByteInBuffer(pointer, rgb, x1Mask);
			pointer++;

			// Full bytes (if any)
			while (pointer < last) {
				setByteInBuffer(pointer, rgb);
				pointer++;
			}

			// Last byte
			setByteInBuffer(pointer, rgb, x2Mask);
		}
	}

	__attribute__((flatten))
	virtual void drawFastVLine(int16_t x_, int16_t y1_, int16_t h, uint16_t color) override
	{
		if (x_ < 0 || x_ >= constWidth || y1_ >= constHeight || h <= 0)
			return;
		if (y1_ < 0) {
			h += y1_;
			if (h <= 0)
				return;
			y1_ = 0;
		}
		const auto y2_ = std::min(y1_ + h - 1, constHeight - 1);

		// Casting to unsigned and fast types really helps here a tiny bit.
		// Also, allow for flipping; note X axis is flipped by default.
		const uint_fast16_t x = flipX ? x_ : constWidth - 1 - x_;
		const uint_fast16_t y1 = flipY ? constHeight - 1 - y1_ : y1_;
		const uint_fast16_t y2 = flipY ? constHeight - 1 - y2_ : y2_;

		const uint_fast16_t xByte = x / 8;
		const uint_fast8_t xBit = x % 8;
		const uint8_t mask = 1 << xBit;

		const auto rgb = prepareColorComponents(color);
		for (uint_fast16_t y = y1; y <= y2; y++) {
			uint8_t* pointer = getRowPointer(y) - xByte;
			setByteInBuffer(pointer, rgb, mask);
		}
	}

	////////////////////////////////////////
	// Drawing support
private:

	struct RGBComponents
	{
		uint_fast8_t r;
		uint_fast8_t g;
		uint_fast8_t b;
	};

	/// Converts RGB565 to components with 5 bit precision (only 5 LSB used),
	/// then down to defined color depth. 6th bit of green is always ignored.
	__attribute__((always_inline))
	inline RGBComponents prepareColorComponents(uint16_t color) const
	{
		const uint_fast8_t r = color >> (11 + (5 - constColorDepth));
		const uint_fast8_t g = color >>  (6 + (5 - constColorDepth));
		const uint_fast8_t b = color >>  (0 + (5 - constColorDepth));
		return { r, g, b };
	}

	static constexpr auto rOffset = -patternColorBytes * 0;
	static constexpr auto gOffset = -patternColorBytes * 1;
	static constexpr auto bOffset = -patternColorBytes * 2;

	__attribute__((always_inline))
	inline void setByteInBuffer(uint8_t* pointer, const RGBComponents& c)
	{
		#pragma GCC unroll 5
		for (uint_fast8_t i = 0; i < constColorDepth; i++) {
			const size_t depthBufferOffset = noDepthBufferSize * i;

			pointer[depthBufferOffset + rOffset] = ((c.r >> i) & 1) ? 0xFF : 0x00;
			pointer[depthBufferOffset + gOffset] = ((c.g >> i) & 1) ? 0xFF : 0x00;
			pointer[depthBufferOffset + bOffset] = ((c.b >> i) & 1) ? 0xFF : 0x00;
		}
	}

	__attribute__((always_inline))
	inline void setByteInBuffer(uint8_t* pointer, const RGBComponents& c, uint8_t mask)
	{
		#pragma GCC unroll 5
		for (uint_fast8_t i = 0; i < constColorDepth; i++) {
			const size_t depthBufferOffset = noDepthBufferSize * i;

			if ((c.r >> i) & 1)
				pointer[depthBufferOffset + rOffset] |= mask;
			else
				pointer[depthBufferOffset + rOffset] &= ~mask;

			if ((c.g >> i) & 1)
				pointer[depthBufferOffset + gOffset] |= mask;
			else
				pointer[depthBufferOffset + gOffset] &= ~mask;

			if ((c.b >> i) & 1)
				pointer[depthBufferOffset + bOffset] |= mask;
			else
				pointer[depthBufferOffset + bOffset] &= ~mask;
		}
	}

	////////////////////////////////////////
	// Display driving

public:
	/// Updates the display by minimal step (single minimal chunk).
	/// The `showTime` (in CPU cycles) scales with the currently displayed 
	/// color depth. It's kinda half of actual show time on average in long run.
	__attribute__((flatten))
	void displayStep(uint16_t showTime)
	{
		setMux(displayRowPattern);
		pulseLatch();

#ifdef DISPLAY_FAST_UPDATE_OPTIMIZATION
#	ifndef DISPLAY_CYCLES_FOR_UPDATE
#		define DISPLAY_CYCLES_FOR_UPDATE 5871ULL * F_CPU / 1000000000 // from my experiments on ESP8266
#	endif
		static constexpr uint16_t cyclesForUpdate = DISPLAY_CYCLES_FOR_UPDATE;
		unsigned long expected = (showTime >> 2) * (1 << displayColorDepth);
		if (expected >= cyclesForUpdate) {
			expected -= cyclesForUpdate;
			enableOutput();
		}
#else
		enableOutput();
#endif

#ifdef DEBUG_DISPLAY_SHOW_TIME
		unsigned long start = micros();
#endif

		SPI.writeBytes(displayNextBufferPosition, sendBufferSize);

		displayRowPattern += 1;
		if (displayRowPattern >= constRowPattern) {
			displayRowPattern = 0;
			displayColorDepth += 1;
			if (displayColorDepth >= constColorDepth) {
				displayColorDepth = 0;
			}
		}

		if (displayRowPattern == constRowPattern - 1 && displayColorDepth == constColorDepth - 1) {
			displayNextBufferPosition = buffer;
		}
		else {
			displayNextBufferPosition += sendBufferSize;
		}

#ifdef DEBUG_DISPLAY_SHOW_TIME
		// TODO: count cycles instead microseconds & try balance `if`s above
		if (collectDebugCounters) {
			unsigned long now = micros() - start;
			showTimeByRowPattern[displayRowPattern] += now;
			showTimeByColorDepth[displayColorDepth] += now;
			showTimeCounter++;
		}
#endif // DEBUG_DISPLAY_SHOW_TIME

		enableOutput();

		// Loop has 4 cycles per loop iteration
		for (unsigned long i = 0; i < expected; i++) {
			asm volatile ("nop");
			asm volatile ("nop");
		}

		disableOutput();

		// Serial.printf("d\trP=%u\tcD=%u\tnBP=%u\tus=%lu\n", 
		// 	displayRowPattern, displayColorDepth, 
		// 	displayNextBufferPosition - buffer, micros() - start);
	}

	void displaySingleColorDepth(uint16_t showTime)
	{
#ifdef ESP8266
		ESP.wdtFeed();
#endif
		do {
			displayStep(showTime);
		} while (displayRowPattern > 0);
	}

	void displayEverything(uint16_t showTime)
	{
		do {
			displaySingleColorDepth(showTime);
		} while (displayColorDepth > 0);
	}

#ifdef DEBUG_DISPLAY_SHOW_TIME
	volatile bool collectDebugCounters =  false;
	size_t showTimeCounter;
	unsigned long showTimeByRowPattern[constRowPattern];
	unsigned long showTimeByColorDepth[constColorDepth];

	void printDebugCounters()
	{
		collectDebugCounters = false;
		Serial.printf("showTimeCounter=%u\n", showTimeCounter);
		for (size_t i = 0; i < constRowPattern; i++) {
			Serial.printf("showTimeByRowPattern[%u]=%lu\n", 
				i, showTimeByRowPattern[i]);
		}
		for (size_t i = 0; i < constColorDepth; i++) {
			Serial.printf("showTimeByColorDepth[%u]=%lu\n", 
				i, showTimeByColorDepth[i]);
		}
		collectDebugCounters = true;
	}

	void resetDebugCounters()
	{
		collectDebugCounters = false;
		showTimeCounter = 0;
		for (size_t i = 0; i < constRowPattern; i++) {
			showTimeByRowPattern[i] = 0;
		}
		for (size_t i = 0; i < constColorDepth; i++) {
			showTimeByColorDepth[i] = 0;
		}
		collectDebugCounters = true;
	}
#endif // DEBUG_DISPLAY_SHOW_TIME

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
		if constexpr (PIN_OE == 16) {
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
		if constexpr (PIN_OE == 16) {
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
