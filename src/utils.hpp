#pragma once

#include <cstdint>
#include <string_view>

#define CONCATENATE_DIRECT(X, Y) X##Y
#define CONCATENATE(X, Y) CONCATENATE_DIRECT(X, Y)
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)

// Utils to help printf as binary
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)   \
	(byte & 0x80 ? '1' : '0'), \
	(byte & 0x40 ? '1' : '0'), \
	(byte & 0x20 ? '1' : '0'), \
	(byte & 0x10 ? '1' : '0'), \
	(byte & 0x08 ? '1' : '0'), \
	(byte & 0x04 ? '1' : '0'), \
	(byte & 0x02 ? '1' : '0'), \
	(byte & 0x01 ? '1' : '0') 

// Taken from great article at https://stackoverflow.com/a/109025/4880243
constexpr uint8_t numberOfSetBits(uint32_t i)
{
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	i = (i + (i >> 4)) & 0x0F0F0F0F;
	return (i * 0x01010101) >> 24;
}


template <typename T>
constexpr typename std::enable_if_t<sizeof(T) == 2, T>
hton(T value) noexcept
{
	return  ((value & 0x00FF) << 8)
	/***/ | ((value & 0xFF00) >> 8);
}

template <typename T>
constexpr typename std::enable_if_t<sizeof(T) == 4, T>
hton(T value) noexcept
{
	return  ((value & 0x000000FF) << 24)
	/***/ | ((value & 0x0000FF00) <<  8)
	/***/ | ((value & 0x00FF0000) >>  8)
	/***/ | ((value & 0xFF000000) >> 24);
}

template <typename T>
constexpr typename std::enable_if_t<sizeof(T) == 8, T>
hton(T value) noexcept
{
	return  ((value & 0xFF00000000000000ull) >> 56)
	/***/ | ((value & 0x00FF000000000000ull) >> 40)
	/***/ | ((value & 0x0000FF0000000000ull) >> 24)
	/***/ | ((value & 0x000000FF00000000ull) >>  8)
	/***/ | ((value & 0x00000000FF000000ull) <<  8)
	/***/ | ((value & 0x0000000000FF0000ull) << 24)
	/***/ | ((value & 0x000000000000FF00ull) << 40)
	/***/ | ((value & 0x00000000000000FFull) << 56);
}


constexpr bool parseBoolean(const char* str) {
	//return str[0] == '1' || str[0] == 't' || str[0] == 'T' || str[0] == 'y' || str[0] == 'Y';
	return !(str[0] == '0' || str[0] == 'f' || str[0] == 'F' || str[0] == 'n' || str[0] == 'N');
}

// Taken from https://stackoverflow.com/a/65765075/4880243
constexpr std::int32_t parseInt(std::string_view str, std::size_t* pos = nullptr) {
	using namespace std::literals;
	const auto numbers = "0123456789"sv;

	const auto begin = str.find_first_of(numbers);
	if (begin == std::string_view::npos)
		return 0;

	const auto sign = begin != 0U && str[begin - 1U] == '-' ? -1 : 1;
	str.remove_prefix(begin);

	const auto end = str.find_first_not_of(numbers);
	if (end != std::string_view::npos)
		str.remove_suffix(std::size(str) - end);

	auto result = 0;
	auto multiplier = 1U;
	for (std::ptrdiff_t i = std::size(str) - 1U; i >= 0; --i) {
		auto number = str[i] - '0';
		result += number * multiplier * sign;
		multiplier *= 10U;
	}

	if (pos != nullptr) *pos = begin + std::size(str);
	return result;
}

/// Parses string view as IPv4 in form of human ordered `uint32_t`.
constexpr std::uint32_t parseIPv4(std::string_view str) {
	size_t pos = 0;
	const auto a = parseInt(str, &pos);
	str.remove_prefix(pos);
	const auto b = parseInt(str, &pos);
	str.remove_prefix(pos);
	const auto c = parseInt(str, &pos);
	str.remove_prefix(pos);
	const auto d = parseInt(str, &pos);
	return (a << 24) | (b << 16) | (c << 8) | d;
}

namespace {
	using namespace std::literals;
	static_assert(parseIPv4("127.0.0.1"sv) == 0x7F000001);
	static_assert(parseIPv4("192.168.55.17"sv) == 3232249617);
	static_assert(hton(parseIPv4("192.168.55.17"sv)) == 288860352);
}

inline unsigned int baseTwoDigits(unsigned int x) {
	return x ? 32 - __builtin_clz(x) : 0;
}

unsigned int baseTenDigits(unsigned int x);
