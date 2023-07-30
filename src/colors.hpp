#include <algorithm> // min, max

namespace colors {

struct RGB {
	uint8_t r, g, b;
};

struct HSL {
	float h, s, l;
};

HSL toHSL(RGB rgb) {
	float r_ = float(rgb.r) / 255;
	float g_ = float(rgb.g) / 255;
	float b_ = float(rgb.b) / 255;

	float maxColor = std::max({r_, g_, b_});
	float minColor = std::min({r_, g_, b_});
	float h, s, l;

	l = (maxColor + minColor) / 2;

	if (maxColor == minColor) {
		h = s = 0;
	} 
	else {
		float d = maxColor - minColor;
		s = l > 0.5F ? d / (2.0F - maxColor - minColor) : d / (maxColor + minColor);
		if (maxColor == r_) {
			h = (g_ - b_) / d + (g_ < b_ ? 6 : 0);
		} 
		else if (maxColor == g_) {
			h = (b_ - r_) / d + 2;
		} 
		else {
			h = (r_ - g_) / d + 4;
		}
		h *= 60;
	}

	return {h, s * 100, l * 100};
}

namespace {
	float hue2RGB(float p, float q, float t) {
		if (t < 0.0) t += 1.0;
		if (t > 1.0) t -= 1.0;
		if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
		if (t < 1.0 / 2.0) return q;
		if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
		return p;
	}
}

RGB toRGB(HSL hsl) {
	float h_ = hsl.h / 360;
	float s_ = hsl.s / 100;
	float l_ = hsl.l / 100;

	float q = l_ < 0.5 ? l_ * (1.0 + s_) : l_ + s_ - l_ * s_;
	float p = 2.0 * l_ - q;

	float r = hue2RGB(p, q, h_ + 1.0 / 3.0);
	float g = hue2RGB(p, q, h_);
	float b = hue2RGB(p, q, h_ - 1.0 / 3.0);

	return {
		static_cast<uint8_t>(r * 255), 
		static_cast<uint8_t>(g * 255), 
		static_cast<uint8_t>(b * 255)
	};
}

inline uint16_t to565(RGB rgb) {
	return ((rgb.r & 0b11111000) << 8) | ((rgb.g & 0b11111100) << 3) | (rgb.b >> 3);
}

RGB interpolateRGB(const RGB& a, const RGB& b, float ratio) {
	return {
		static_cast<uint8_t>(a.r + (b.r - a.r) * ratio),
		static_cast<uint8_t>(a.g + (b.g - a.g) * ratio),
		static_cast<uint8_t>(a.b + (b.b - a.b) * ratio),
	};
}

HSL interpolateHSL(const HSL& a, const HSL& b, float ratio) {
	return {
		a.h + (b.h - a.h) * ratio,
		a.s + (b.s - a.s) * ratio,
		a.l + (b.l - a.l) * ratio,
	};
}

}