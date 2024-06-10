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

enum class Mode : uint8_t
{
	None,
	Steps,
	SingleColorDepth,
	Everything,
};

Mode mode = Mode::SingleColorDepth;
uint8_t interval = 4;
uint16_t showTime = 100;
#ifdef DEBUG_DISPLAY_SHOW_TIME
unsigned long displayTickCounter = 0;
unsigned long displayTickTimeSum = 0;
#endif
uint8_t example = 1;

/// Setups display ticker for specified settings.
/// The `interval` is in milliseconds, `showTime` is in CPU cycles.
void setupDisplayTicker(Mode mode, uint8_t interval, uint16_t showTime)
{
	displayTicker.detach();
	switch (mode) {
		case Mode::Steps:
			displayTicker.attach_ms(interval, [showTime] {
#ifdef DEBUG_DISPLAY_SHOW_TIME
				unsigned long now = micros();
				display.displayStep(showTime);
				displayTickTimeSum += micros() - now;
				displayTickCounter++;
#else
				display.displayStep(showTime);
#endif
			});
			break;
		case Mode::SingleColorDepth:
			displayTicker.attach_ms(interval, [showTime] {
#ifdef DEBUG_DISPLAY_SHOW_TIME
				unsigned long now = micros();
				display.displaySingleColorDepth(showTime);
				displayTickTimeSum += micros() - now;
				displayTickCounter++;
#else
				display.displaySingleColorDepth(showTime);
#endif
			});
			break;
		case Mode::Everything:
			displayTicker.attach_ms(interval, [showTime] {
#ifdef DEBUG_DISPLAY_SHOW_TIME
				unsigned long now = micros();
				display.displayEverything(showTime);
				displayTickTimeSum += micros() - now;
				displayTickCounter++;
#else
				display.displayEverything(showTime);
#endif
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
		float hue = static_cast<float>(x * 360) / display.width();
		display.drawFastVLine(x, 0, display.height(), to565(HSL{hue, 100, 50}));
	}
}

void drawVerticalGradient()
{
	for (int y = 0; y < display.height(); y++) {
		float hue = static_cast<float>(y * 360) / display.height();
		display.drawFastHLine(0, y, display.width(), to565(HSL{hue, 100, 50}));
	}
}

void draw2DGradient()
{
	for (int x = 0; x < display.width(); x++) {
		float hue = static_cast<float>(x * 360) / display.width();
		for (int y = 0; y < display.height(); y++) {
			float saturation = static_cast<float>(y * 100) / display.height();
			display.drawPixel(x, y, to565(HSL{hue, saturation, 50}));
		}
	}
}

void drawThreeStripesAngled()
{
	display.fillScreen(0b0000100001000001);
	display.drawLine(0, 0, display.width(), display.height(), 0b0000011111100000);
	display.drawLine(display.width() / 2, 0, display.width(), display.height() / 2, 0b1111100000000000);
	display.drawLine(0, display.height() / 2, display.width() / 2, display.height(), 0b0000000000011111);
}

void drawSingleColorGradients(uint8_t shift)
{
	for (int x = 0; x < 32; x++) {
		display.drawLine(x, 0, x, display.height(), x << shift);
	}
	for (int y = 0; y < 32; y++) {
		display.drawLine(32, y, display.width(), y, y << shift);
	}
}

void drawWhiteGradients()
{
	for (int x = 0; x < 32; x++) {
		display.drawLine(x, 0, x, display.height(), (x << 11) | (x << 6) | x);
	}
	for (int y = 0; y < 32; y++) {
		display.drawLine(32, y, display.width(), y, (y << 11) | (y << 6) | y);
	}
}

void drawOrthogonalLines()
{
	display.fillScreen(0b0000100001000001);
	for (auto& i : {23, 17, 13, 9, 7, 5, 3, 0}) {
		const auto w = 2 + i * 2;
		const auto h = 2 + i * 3 / 2;
		const uint16_t c = colors::to565(colors::HSL{static_cast<float>(i) * 20, 100, 50});
		display.drawRect(i * 3 / 2, display.height() - w, w, h, c);
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
							"displayTickCounter=%lu\ndisplayTickTimeSum=%lu\n", 
							displayTickCounter, displayTickTimeSum);
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
	// When conversion is complete, update thermometer
	if (oneWire.read_bit()) {
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
		char buffer[8];
		char* p = buffer;
		if (raw >= 10) *p++ = '0' + (raw / 10);
		*p++ = '0' + (raw % 10);
		*p++ = '.';
		*p++ = digitAfterComma;
		*p++ = '\'';
		*p++ = 'C';
		*p = 0;

		now = micros() - now;
		Serial.print(F("getTempC: ")); Serial.print(now);
		now = micros();

		optimistic_yield(1024);

		now = micros() - now;
		Serial.print(F(" after 1st yield: ")); Serial.print(now);
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
		Serial.print(F(" after 2nd yield: ")); Serial.print(now);
		now = micros();

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
		}
		display.setTextColor(0);
		display.setCursor(1, 1);
		display.print(buffer);

		now = micros() - now;
		Serial.print(F("\tdisplay draw: ")); Serial.println(now);
	}
}
