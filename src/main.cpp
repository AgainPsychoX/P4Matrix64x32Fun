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

enum class Mode : uint8_t
{
	None,
	Steps,
	SingleColorDepth,
	Everything,
};

Mode mode = Mode::SingleColorDepth;
uint8_t interval = 4;
uint8_t baseShowTime = 0;
uint8_t depthStepShowTime = 16;

/// Setups display ticker for specified settings.
/// The `interval` is in milliseconds, 
/// the `baseShowTime` and `depthStepShowTime` are in microseconds.
void setupDisplayTicker(Mode mode, uint8_t interval, uint8_t baseShowTime, uint8_t depthStepShowTime)
{
	displayTicker.detach();

	// Warn about invalid show-time and interval ratio, which causes hang ups
	auto expected = baseShowTime + depthStepShowTime * (1 << display.colorDepth());
	switch (mode) {
		case Mode::Steps: break;
		case Mode::SingleColorDepth: expected *= 2; break;
		case Mode::Everything: expected *= 2 * display.colorDepth(); break;
	}
	if (interval * 1000 <= expected) {
		Serial.println(F("Display interval might be too small for specified show time"));
	}
	const auto p = static_cast<float>(-expected) / static_cast<float>(interval * 10);
	Serial.printf("Estimated performance hit: %.2f%%\n", p);

	switch (mode) {
		case Mode::Steps:
			displayTicker.attach_ms(interval, [baseShowTime, depthStepShowTime] {
				display.displayStep(baseShowTime, depthStepShowTime);
			});
			break;
		case Mode::SingleColorDepth:
			displayTicker.attach_ms(interval, [baseShowTime, depthStepShowTime] {
				display.displaySingleColorDepth(baseShowTime, depthStepShowTime);
			});
			break;
		case Mode::Everything:
			displayTicker.attach_ms(interval, [baseShowTime, depthStepShowTime] {
				display.displayEverything(baseShowTime, depthStepShowTime);
			});
			break;
		default:
			// No ticking, no display
			break;
	}
}

void setup()
{
	delay(1000);

	// Initialize Serial console
	Serial.begin(115200);
	Serial.println(F("\033[2J\nHello!")); // clears serial output garbage
	delay(1000);

	// Initialize display
	display.begin();
	display.fillScreen(0); // black
	setupDisplayTicker(mode, interval, baseShowTime, depthStepShowTime);
#ifdef DEBUG_DISPLAY_SHOW_TIME
	display.resetDebugCounters();
#endif
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
	display.fillScreen(0);
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

}

////////////////////////////////////////////////////////////////////////////////

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
						setupDisplayTicker(mode, interval, baseShowTime, depthStepShowTime);
					}
					else if (line[0] == 'b') {
						baseShowTime = strtoul(p + 1, nullptr, 10);
						setupDisplayTicker(mode, interval, baseShowTime, depthStepShowTime);
					}
					else if (line[0] == 'd') {
						depthStepShowTime = strtoul(p + 1, nullptr, 10);
						setupDisplayTicker(mode, interval, baseShowTime, depthStepShowTime);
					}
					else if (line[0] == 'm') {
						mode = static_cast<Mode>(strtoul(p + 1, nullptr, 10));
						setupDisplayTicker(mode, interval, baseShowTime, depthStepShowTime);
					}
					else if (line[0] == 'e') {
						unsigned int example = strtoul(p + 1, nullptr, 10);
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
							default:
								Serial.println(F("Example not found"));
								break;
						}
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
}
