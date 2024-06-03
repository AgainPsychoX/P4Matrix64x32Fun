#include <MyPxMatrix.hpp>

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

void setup()
{
	delay(1000);

	// Initialize Serial console
	Serial.begin(115200);
	Serial.println(F("\033[2J\nHello!")); // clears serial output garbage
	delay(1000);

	display.begin();
	// display.fillScreen(0b0000011111100000);
	display.drawLine(0, 0, display.width(), display.height(), 0b0000011111100000);
}

unsigned long millisBetweenSteps = 4;
uint8_t minimalShowTime = 4;
enum class Mode { Steps, Color, Whole } mode;

void loop()
{
	display.displayStep(4);
	delay(4);
}
