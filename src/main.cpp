#include <OneWire.h>
#include <MyPxMatrix.hpp>
#include <Ticker.h>
#include <colors.hpp>

MyPxMatrix<
	16,         // PIN_LATCH
	2,          // PIN_OE
	5,          // PIN_A
	4,          // PIN_B
	15,         // PIN_C
	12,         // PIN_D
	-1,         // PIN_E 
	64,         // width
	32,         // height
	8,          // rowPattern
	5,          // colorDepth
	20000000    // spiFrequency
> display;

Ticker displayTicker;

OneWire oneWire;
union OneWireDeviceAddress 
{
	uint8_t raw[8];
	struct {
		uint8_t family;
		uint8_t serial[6];
		uint8_t crc;
	};

	operator uint8_t* () { return raw; }
};
constexpr bool singleOneWireDevice = true;

OneWireDeviceAddress thermometerAddress; // the single DS18B20
constexpr uint8_t thermometerResolution = 12;
char temperatureString[8];

enum class Mode : uint8_t
{
	None,
	Steps,
	SingleColorDepth,
	Everything,
};

uint8_t example = 1;
Mode mode = Mode::SingleColorDepth;
uint8_t interval = 4;
unsigned long intervalMicroseconds = interval * 1000;
uint16_t showTime = 100;
#ifdef DEBUG_DISPLAY_SHOW_TIME
unsigned long lastTick;
unsigned long displayLateTickCounter = 0;
unsigned long displayLateTickCutoff = interval * 1024;
unsigned long displayTickCounter = 0;
unsigned long displayTickTimeSum = 0;
#define DEBUG_DISPLAY_TICK_TICK_PRINT Serial.printf("t[%lu]", now - lastTick);
#ifndef DEBUG_DISPLAY_TICK_TICK_PRINT
#define DEBUG_DISPLAY_TICK_TICK_PRINT 
#endif
#define DEBUG_DISPLAY_TICK_LATE_TICK_PRINT Serial.printf("LATE");
#ifndef DEBUG_DISPLAY_TICK_LATE_TICK_PRINT
#define DEBUG_DISPLAY_TICK_LATE_TICK_PRINT 
#endif
#	define DEBUG_DISPLAY_TICK_COMMON_CODE_PRE                  \
				unsigned long now = micros();                  \
				unsigned long diff = now - lastTick;           \
				DEBUG_DISPLAY_TICK_TICK_PRINT;                 \
				if (diff < intervalMicroseconds)               \
					return;                                    \
				if (diff > displayLateTickCutoff) {            \
					displayLateTickCounter++;                  \
					DEBUG_DISPLAY_TICK_LATE_TICK_PRINT;        \
				}                                              \
				lastTick = now;
#	define DEBUG_DISPLAY_TICK_COMMON_CODE_POST                 \
				displayTickTimeSum += micros() - now;          \
				displayTickCounter++;
#else
#	define DEBUG_DISPLAY_TICK_COMMON_CODE_PRE
#	define DEBUG_DISPLAY_TICK_COMMON_CODE_POST
#endif

extern "C" {
#include "cont.h"
}
extern cont_t* g_pcont;
extern "C" void esp_schedule();
extern "C" void esp_yield();

void pessimisticYieldForDisplayTick(unsigned int maxTime)
{
	// const unsigned long timeSinceLastTick = micros() - lastTick;
	// if (intervalMicroseconds < timeSinceLastTick) {
	// 	// Late tick
	// 	Serial.print('Y');
	// 	yield();
	// 	return;
	// }
	while (true) {
		const unsigned long timeSinceLastTick = micros() - lastTick;
		const unsigned long timeUntilNextTick = intervalMicroseconds - timeSinceLastTick;
		// FIXME: if maxTime is higher than tick interval, there can be infinite loop
		if (maxTime < timeUntilNextTick)
			break;
		Serial.printf("y@%lu;", timeSinceLastTick);
		// yield();
		esp_schedule(); // without it the original loop task doesn't continue after yield
		// esp_yield(); // == `if (can_yield()) esp_yield_within_cont();`
		cont_yield(g_pcont);
	}
}

/// Setups display ticker for specified settings.
/// The `interval` is in milliseconds, `showTime` is in CPU cycles.
void setupDisplayTicker(Mode mode, uint8_t interval, uint16_t showTime)
{
	displayTicker.detach();
	switch (mode) {
		case Mode::Steps:
			displayTicker.attach_ms(interval, [showTime] {
				DEBUG_DISPLAY_TICK_COMMON_CODE_PRE;
				display.displayStep(showTime);
				DEBUG_DISPLAY_TICK_COMMON_CODE_POST
			});
			break;
		case Mode::SingleColorDepth:
			displayTicker.attach_ms(interval, [showTime] {
				DEBUG_DISPLAY_TICK_COMMON_CODE_PRE;
				display.displaySingleColorDepth(showTime);
				DEBUG_DISPLAY_TICK_COMMON_CODE_POST;
			});
			break;
		case Mode::Everything:
			displayTicker.attach_ms(interval, [showTime] {
				DEBUG_DISPLAY_TICK_COMMON_CODE_PRE;
				display.displayEverything(showTime);
				DEBUG_DISPLAY_TICK_COMMON_CODE_POST;
			});
			break;
		default:
			// No ticking, no display
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////

namespace examples {

using namespace colors;

void drawHorizontalGradient()
{
	for (int x = 0; x < display.width(); x++) {
		unsigned long now = micros();
		float hue = static_cast<float>(x) / display.width();
		display.drawFastVLine(x, 0, display.height(), to565(HSL{hue, 1, 0.5}));
		pessimisticYieldForDisplayTick(micros() - now + 100);
	}
}

void drawVerticalGradient()
{
	for (int y = 0; y < display.height(); y++) {
		unsigned long now = micros();
		float hue = static_cast<float>(y) / display.height();
		display.drawFastHLine(0, y, display.width(), to565(HSL{hue, 1, 0.5}));
		pessimisticYieldForDisplayTick(micros() - now + 100);
	}
}

void draw2DGradient()
{
	unsigned int t = micros() >> 10 & 0xFFF;
	for (unsigned int x = 0; x < 64; x++) {
		unsigned long now = micros();
		float hue = static_cast<float>(((x << 6) + t) & 0xFFF) / 4096;
		for (unsigned int y = 0; y < 32; y++) {
			float saturation = static_cast<float>(y) / 32;
			display.drawPixel(x, y, to565(HSL{hue, saturation, 0.5}));
		}
		pessimisticYieldForDisplayTick(micros() - now + 100);
	}
}

void drawThreeStripesAngled()
{
	display.fillScreen(0b0000100001000001);
	yield();
	display.drawLine(0, 0, display.width(), display.height(), 0b0000011111100000);
	display.drawLine(display.width() / 2, 0, display.width(), display.height() / 2, 0b1111100000000000);
	display.drawLine(0, display.height() / 2, display.width() / 2, display.height(), 0b0000000000011111);
}

void drawSingleColorGradients(uint8_t shift)
{
	for (int x = 0; x < 32; x++) {
		display.drawLine(x, 0, x, display.height(), x << shift);
		if (x & 0b111) yield();
	}
	for (int y = 0; y < 32; y++) {
		display.drawLine(32, y, display.width(), y, y << shift);
		if (y & 0b111) yield();
	}
}

void drawWhiteGradients()
{
	for (int x = 0; x < 32; x++) {
		display.drawLine(x, 0, x, display.height(), (x << 11) | (x << 6) | x);
		if (x & 0b111) yield();
	}
	for (int y = 0; y < 32; y++) {
		display.drawLine(32, y, display.width(), y, (y << 11) | (y << 6) | y);
		if (y & 0b111) yield();
	}
}

void drawOrthogonalLines()
{
	display.fillScreen(0b0000100001000001);
	for (auto& i : {23, 17, 13, 9, 7, 5, 3, 0}) {
		const auto w = 2 + i * 2;
		const auto h = 2 + i * 3 / 2;
		const uint16_t c = colors::to565(colors::HSL{static_cast<float>(i) / 20, 1, 0.5});
		display.drawRect(i * 3 / 2, display.height() - w, w, h, c);
	}
}

void drawFilledRectangles()
{
	display.fillScreen(0b0000100001000001);
	for (auto& i : {23, 17, 13, 9, 7, 5, 3, 0}) {
		const auto w = 2 + i * 2;
		const auto h = 2 + i * 3 / 2;
		const uint16_t c = colors::to565(colors::HSL{static_cast<float>(i) / 20, 1, 0.5});
		display.fillRect(i * 3 / 2, display.height() - w, w, h, c);
		if (i > 5) yield();
	}
}

}

////////////////////////////////////////////////////////////////////////////////

void setup()
{
	delay(1000);

	// Initialize Serial console
	Serial.begin(921600);
	Serial.println(F("\033[2J\nHello!")); // clears serial output garbage
	delay(1000);

	// Initialize display
	display.begin();
	// display.fillScreen(0); // black
	examples::drawSingleColorGradients(0);
	setupDisplayTicker(mode, interval, showTime);
#ifdef DEBUG_DISPLAY_SHOW_TIME
	display.resetDebugCounters();
#endif

	// Initialize the thermometer
	{
		oneWire.begin(D3); // prepares the OneWire, incl. resetting search
		oneWire.search(thermometerAddress); // extra dummy search sometimes required idk why
		oneWire.reset_search();

		bool found = false;
		while (oneWire.search(thermometerAddress)) {
			Serial.printf_P(PSTR("[OneWire] Found device, family %02x\n"), thermometerAddress.family);
			if (oneWire.crc8(thermometerAddress, 7) == thermometerAddress.crc) {
				if (thermometerAddress.family == 0x28 /* DS18B20 */) {
					found = true;
					break;
				}
			}
		}
		if (found) {
			Serial.println(F("[Temperature] DS18B20 found"));

			// Read scratch pad
			uint8_t scratchPad[9];
			oneWire.reset();
			if (singleOneWireDevice)
				oneWire.skip();
			else
				oneWire.select(thermometerAddress);
			oneWire.write(0xBE);
			oneWire.read_bytes(scratchPad, 9);

			// Print scratch pad for debugging
			Serial.print(F("[Temperature] Scratch pad: "));
			for (unsigned int i = 0; i < sizeof(scratchPad); i++) {
				Serial.printf_P(PSTR("%02X "), scratchPad[i]);
			}
			Serial.println();

			// Write scratch pad with selected resolution
			oneWire.reset();
			if (singleOneWireDevice)
				oneWire.skip();
			else
				oneWire.select(thermometerAddress);
			oneWire.write(0x4E);
			oneWire.write(scratchPad[2]); // pass low alarm temperature
			oneWire.write(scratchPad[3]); // pass high alarm temperature
			oneWire.write(0b11111 | ((thermometerResolution - 9) << 5));

			// Start conversion
			oneWire.reset();
			if (singleOneWireDevice)
				oneWire.skip();
			else
				oneWire.select(thermometerAddress);
			oneWire.write(0x44);

			Serial.println(F("[Temperature] DS18B20 setup done"));
		}
		else {
			Serial.println(F("[Temperature] DS18B20 missing"));
		}

		// Initialize temperature string as empty
		temperatureString[0] = 0;
	}
}

void loop()
{
	// Parse serial commands
	static char line[16];
	static size_t lineLength = 0;
	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\r' || c == '\n') {
			if (lineLength > 0) {
				line[lineLength] = '\0';
				char* p = line;
				while (*p && *p != '=') p++;
				if (*p) {
					if (line[0] == 'i') {
						interval = strtoul(p + 1, nullptr, 10);
						intervalMicroseconds = interval * 1000;
#ifdef DEBUG_DISPLAY_SHOW_TIME
						displayLateTickCutoff = interval * 1024;
#endif
						setupDisplayTicker(mode, interval, showTime);
					}
					else if (line[0] == 't') {
						showTime = strtoul(p + 1, nullptr, 10);
						setupDisplayTicker(mode, interval, showTime);
					}
					else if (line[0] == 'm') {
						mode = static_cast<Mode>(strtoul(p + 1, nullptr, 10));
						setupDisplayTicker(mode, interval, showTime);
					}
					else if (line[0] == 'e') {
						example = strtoul(p + 1, nullptr, 10);
					}
#ifdef DEBUG_DISPLAY_SHOW_TIME
					else if (line[0] == 'l') {
						displayLateTickCutoff = strtoul(p + 1, nullptr, 10);
					}
#endif
					else {
						Serial.println(F("Unknown assignment"));
					}
				}
				else /* not assignment */ {
					if (line[0] == 'd' && line[1] == 'c') {
#ifdef DEBUG_DISPLAY_SHOW_TIME
						display.printDebugCounters();
						display.resetDebugCounters();

						Serial.printf(
							"displayLateTickCounter=%lu\n"
							"displayTickCounter=%lu\n"
							"displayTickTimeSum=%lu\n",
							displayLateTickCounter,
							displayTickCounter, 
							displayTickTimeSum);
						displayLateTickCounter = 0;
						displayTickCounter = 0;
						displayTickTimeSum = 0;
#endif
					}
					else {
						Serial.println(F("Unknown command"));
					}
				}
			}
			lineLength = 0;
		}
		else if (lineLength < sizeof(line) - 1) {
			line[lineLength++] = c;
		}
		else {
			Serial.println(F("Line too long"));
			lineLength = 0;
		}
	}

	unsigned long now = micros();
	bool justUpdatedThermometer = false;

	// When conversion is complete, update thermometer
	if (oneWire.read_bit()) {
		justUpdatedThermometer = true;

		// Read temperature fast (2 first bytes of scratch pad)
		// This will skip CRC.
		oneWire.reset();
		if (singleOneWireDevice)
			oneWire.skip();
		else
			oneWire.select(thermometerAddress);
		oneWire.write(0xBE);
		uint8_t lsb = oneWire.read();
		uint8_t msb = oneWire.read();
		int16_t raw = (msb << 8) | lsb;

		// Convert
		bool sign = msb >> 7;
		raw = sign ? -raw : raw;
		static const char digitAfterCommaLookup[16] = {
			'0', // 0       
			'1', // 0.0625  
			'1', // 0.125   
			'2', // 0.1875  
			'3', // 0.25    
			'3', // 0.3125  
			'4', // 0.375   
			'4', // 0.4375  
			'5', // 0.5     
			'6', // 0.5625  
			'6', // 0.625   
			'7', // 0.6875  
			'8', // 0.75    
			'8', // 0.8125  
			'9', // 0.875   
			'9', // 0.9375  
		};
		char digitAfterComma = digitAfterCommaLookup[raw & 0b1111];
		raw = raw >> 4;
		char* p = temperatureString;
		if (raw >= 10) *p++ = '0' + (raw / 10);
		*p++ = '0' + (raw % 10);
		*p++ = '.';
		*p++ = digitAfterComma;
		*p++ = '\'';
		*p++ = 'C';
		*p = 0;

		now = micros() - now;
		Serial.print(F("got temperature: ")); Serial.print(now);
		now = micros();

		yield();

		now = micros() - now;
		Serial.print(F("\tafter 1st yield: ")); Serial.print(now);
		now = micros();

		// Start conversion
		oneWire.reset();
		if (singleOneWireDevice)
			oneWire.skip();
		else
			oneWire.select(thermometerAddress);
		oneWire.write(0x44);

		now = micros() - now;
		Serial.print(F("\trequestTemperatures: ")); Serial.print(now);
		now = micros();

		yield();

		now = micros() - now;
		Serial.print(F("\tafter 2nd yield: ")); Serial.print(now);
		now = micros();
	}

	// Update display
	switch (example) {
		case 0: display.fillScreen(0); break;
		case 1: examples::drawHorizontalGradient(); break;
		case 2: examples::drawVerticalGradient(); break;
		case 3: examples::draw2DGradient(); break;
		case 4: examples::drawThreeStripesAngled(); break;
		case 5: examples::drawSingleColorGradients(11); break;
		case 6: examples::drawSingleColorGradients(6); break;
		case 7: examples::drawSingleColorGradients(0); break;
		case 8: examples::drawWhiteGradients(); break;
		case 9: examples::drawOrthogonalLines(); break;
		case 10: examples::drawFilledRectangles(); break;
	}

	if (justUpdatedThermometer) {
		now = micros() - now;
		Serial.print(F("\tbackground draw: ")); Serial.print(now);
		now = micros();
	}

	pessimisticYieldForDisplayTick(400);

	display.setTextColor(0);
	display.setCursor(1, 1);
	display.print(temperatureString);
#ifdef DISPLAY_DOUBLE_BUFFER
	display.swapBuffer();
#endif

	if (justUpdatedThermometer) {
		now = micros() - now;
		Serial.print(F("\ttext draw: ")); Serial.print(now);
		now = micros();
	}

	// pessimisticYieldForDisplayTick(500);
	// yield();

	if (justUpdatedThermometer) {
		now = micros() - now;
		Serial.print(F("\tafter yield: ")); Serial.println(now);
	}

	Serial.println('d');
}
