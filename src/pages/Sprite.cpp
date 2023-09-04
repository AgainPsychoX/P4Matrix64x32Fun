#include "Sprite.hpp"

namespace pages {

#ifdef NICE_CODE
uint16_t Sprite::Temperature::interpolateColor(float temperature) {
	using namespace colors;

	if (temperature <= referenceValue[0]) {
		return targetColors[0]; // first color
	}

	for (uint8_t i = 1; i < 4; i++) {
		auto const& previousTemperature = referenceValue[i - 1];
		auto const& previousColor = targetColors[i - 1];
		auto const& currentTemperature = referenceValue[i];
		auto const& currentColor = targetColors[i];

		if (temperature <= currentTemperature) {
			float ratio = (temperature - previousTemperature) 
				/ (currentTemperature - previousTemperature);
			return to565(toRGB(interpolateHSL(
				toHSL(previousColor), toHSL(currentColor), ratio)));
		}
	}

	return targetColors[3]; // last color 
}
#else
uint16_t Sprite::Temperature::interpolateColor(float temperature) {
	using namespace colors;
	static_assert(sizeof(referenceValue) / sizeof(referenceValue[0]) == 3);

	if (temperature <= referenceValue[0]) {
		return targetColors[0]; // first color
	}
	if (temperature > referenceValue[2]) {
		return targetColors[2];
	}

	uint8_t i = temperature <= referenceValue[1] ? 1 : 2;

	auto const& previousTemperature = referenceValue[i - 1];
	auto const& previousColor = targetColors[i - 1];
	auto const& currentTemperature = referenceValue[i];
	auto const& currentColor = targetColors[i];

	float ratio = (temperature - previousTemperature) 
		/ (currentTemperature - previousTemperature);
	return to565(toRGB(interpolateHSL(
		toHSL(previousColor), toHSL(currentColor), ratio)));

	return targetColors[3]; // last color 
}
#endif

}
