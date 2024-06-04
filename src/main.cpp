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
	4,          // colorDepth
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

Mode mode = Mode::Steps;
uint8_t interval = 4;
uint8_t minimalShowTime = 4;

void setupDisplayTicker(Mode mode, uint8_t interval, uint8_t minimalShowTime)
{
	displayTicker.detach();
	switch (mode) {
		case Mode::Steps:
			displayTicker.attach_ms(interval, [minimalShowTime] {
				display.displayStep(minimalShowTime);
			});
			break;
		case Mode::SingleColorDepth:
			displayTicker.attach_ms(interval, [minimalShowTime] {
				display.displaySingleColorDepth(minimalShowTime);
			});
			break;
		case Mode::Everything:
			displayTicker.attach_ms(interval, [minimalShowTime] {
				display.displayEverything(minimalShowTime);
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
	setupDisplayTicker(mode, interval, minimalShowTime);
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
				if (line[0] == 'i' && lineLength >= 2) {
					interval = strtoul(line + 2, nullptr, 10);
					setupDisplayTicker(mode, interval, minimalShowTime);
				}
				else if (line[0] == 't' && lineLength >= 2) {
					minimalShowTime = strtoul(line + 2, nullptr, 10);
					setupDisplayTicker(mode, interval, minimalShowTime);
				}
				else if (line[0] == 'm' && lineLength >= 2) {
					mode = static_cast<Mode>(strtoul(line + 2, nullptr, 10));
					setupDisplayTicker(mode, interval, minimalShowTime);
				}
				else if (line[0] == 'e' && lineLength >= 2) {
					unsigned int example = strtoul(line + 2, nullptr, 10);
					switch (example) {
						case 0:
							display.fillScreen(0);
							break;
						case 1:
							examples::drawHorizontalGradient();
							break;
						case 2:
							examples::drawVerticalGradient();
							break;
						case 3:
							examples::draw2DGradient();
							break;
						case 4:
							examples::drawThreeStripesAngled();
							break;
						default:
							Serial.println(F("Example not found"));
							break;
					}
				}
				else if (line[0] == '?') {
					Serial.print(F("interval="));
					Serial.println(interval);
					Serial.print(F("minimalShowTime="));
					Serial.println(minimalShowTime);
					Serial.print(F("mode="));
					Serial.println(static_cast<int>(mode));
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
