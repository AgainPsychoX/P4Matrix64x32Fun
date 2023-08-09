#include "utils.hpp"

unsigned int baseTenDigits(unsigned int x) {
	static const unsigned char guess[33] = {
		0, 0, 0, 0, 1, 1, 1, 2, 2, 2,
		3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
		6, 6, 6, 6, 7, 7, 7, 8, 8, 8,
		9, 9, 9
	};
	static const unsigned int tenToThe[] = {
		1, 10, 100, 1000, 10000, 100000, 
		1000000, 10000000, 100000000, 1000000000,
	};
	unsigned int digits = guess[baseTwoDigits(x)];
	return digits + (x >= tenToThe[digits]);
}
