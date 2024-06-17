#include <OneWire.h>
#include <MyPxMatrix.hpp>
#include <Schedule.h>
#include <colors.hpp>
#include <utils.hpp> // saturatedSubtract

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
uint16_t interval = 4000; // us
uint16_t showTime = 100; // us
unsigned long lastTick;
#ifdef DEBUG_DISPLAY_SHOW_TIME
unsigned long displayEarlyTickCounter = 0;
unsigned long displayEarlyTickCutoff = interval - 200;
unsigned long displayLateTickCounter = 0;
unsigned long displayLateTickCutoff = interval + 200;
unsigned long displayTickCounter = 0;
unsigned long displayTickTimeSum = 0;
#endif

void displayTick()
{
#ifdef DEBUG_DISPLAY_SHOW_TIME
	unsigned long now = micros();
	unsigned long diff = now - lastTick;
	Serial.printf("t[%lu]", now - lastTick);
	if (diff < displayEarlyTickCutoff) {
		displayEarlyTickCounter++;
		Serial.printf("EARLY");
	}
	if (diff > displayLateTickCutoff) {
		displayLateTickCounter++;
		Serial.printf("LATE");
	}
#endif
	lastTick = now;
	switch (mode) {
		case Mode::Steps:
			display.displayStep(showTime);
			break;
		case Mode::SingleColorDepth:
			display.displaySingleColorDepth(showTime);
			break;
		case Mode::Everything:
			display.displayEverything(showTime);
			break;
		default:
			// No ticking, no display
			break;
	}
#ifdef DEBUG_DISPLAY_SHOW_TIME
	displayTickTimeSum += micros() - now;
	displayTickCounter++;
#endif
}

void pessimisticYieldForDisplayTick(unsigned long maxTimeToWait)
{
	while (true) {
		const unsigned long timeSinceLastTick = micros() - lastTick;
		const auto timeUntilNextTick = saturatedSubtract<unsigned long>(interval, timeSinceLastTick);
		// FIXME: if maxTimeToWait is higher than tick interval, there can be infinite loop
		// Serial.printf("<%lu>", timeSinceLastTick);
		if (maxTimeToWait < timeUntilNextTick)
			break;
		// Serial.print('y');
		Serial.printf("<%lu>y",timeSinceLastTick);
		// displayTick();
		run_scheduled_recurrent_functions();
	}
}

/// Setups display ticker to recur with specified interval.
void setupDisplayTicker(uint16_t myInterval)
{
	schedule_recurrent_function_us([=]() {
		if (myInterval != interval) {
			Serial.printf_P(PSTR("Stopping ticking with interval %uus\n"), myInterval);
			return false; // stops recurring
		}
		displayTick();
		return true; // continues recurring
	}, myInterval);
	Serial.printf_P(PSTR("Starting ticking with interval %uus\n"), myInterval);
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
		float hue = static_cast<float>(((x << 6) + t) & 0xFFF) / 4096;
		for (unsigned int y = 0; y < 32; y++) {
			unsigned long now = micros();
			float saturation = static_cast<float>(y) / 32;
			display.drawPixel(x, y, to565(HSL{hue, saturation, 0.5}));
			pessimisticYieldForDisplayTick(micros() - now + 100);
		}
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
	setupDisplayTicker(interval);
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
#ifdef DEBUG_DISPLAY_SHOW_TIME
						displayLateTickCutoff = interval + 200;
#endif
						setupDisplayTicker(interval);
					}
					else if (line[0] == 't') {
						showTime = strtoul(p + 1, nullptr, 10);
						setupDisplayTicker(interval);
					}
					else if (line[0] == 'm') {
						mode = static_cast<Mode>(strtoul(p + 1, nullptr, 10));
						setupDisplayTicker(interval);
					}
					else if (line[0] == 'e') {
						example = strtoul(p + 1, nullptr, 10);
					}
#ifdef DEBUG_DISPLAY_SHOW_TIME
					else if (line[0] == 'l') {
						const int diff = strtoul(p + 1, nullptr, 10);
						displayEarlyTickCutoff = std::min(interval - diff, 0);
						displayLateTickCutoff = interval + diff;
					}
#endif
					else {
						Serial.println(F("Unknown assignment"));
					}
				}
				else /* not assignment */ {
					if (line[0] == 'c' && line[1] == 'l') {
						Serial.println(F("\033[2J\nHello!"));
					}
					else if (line[0] == 'd' && line[1] == 'c') {
#ifdef DEBUG_DISPLAY_SHOW_TIME
						display.printDebugCounters();
						display.resetDebugCounters();

						Serial.printf(
							"displayEarlyTickCounter=%lu\n"
							"displayLateTickCounter=%lu\n"
							"displayTickCounter=%lu\n"
							"displayTickTimeSum=%lu\n",
							displayEarlyTickCounter,
							displayLateTickCounter,
							displayTickCounter, 
							displayTickTimeSum);
						displayEarlyTickCounter = 0;
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

	pessimisticYieldForDisplayTick(333);

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
	yield();

	if (justUpdatedThermometer) {
		now = micros() - now;
		Serial.print(F("\tafter yield: ")); Serial.println(now);
	}

	Serial.print('d');
	Serial.print(' ');
}
