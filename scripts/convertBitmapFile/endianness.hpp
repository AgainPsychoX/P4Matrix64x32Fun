#include <climits>
#include <cstdint>

template <typename T>
T swap_endian(T u) {
	union {
		T u;
		uint8_t u8[sizeof(T)];
	} source, dest;

	source.u = u;

	for (size_t k = 0; k < sizeof(T); k++)
		dest.u8[k] = source.u8[sizeof(T) - k - 1];

	return dest.u;
}

#if defined __has_builtin

#  if __has_builtin (__builtin_bswap16)
template <>
inline uint16_t swap_endian<uint16_t>(uint16_t value) {
	return __builtin_bswap16(value);
}
#	endif

#  if __has_builtin (__builtin_bswap32)
template <>
inline uint32_t swap_endian<uint32_t>(uint32_t value) {
	return __builtin_bswap32(value);
}
#	endif

#  if __has_builtin (__builtin_bswap64)
template <>
inline uint64_t swap_endian<uint64_t>(uint64_t value) {
	return __builtin_bswap64(value);
}
#	endif

#  if __has_builtin (__builtin_bswap128)
template <>
inline uint128_t swap_endian<uint128_t>(uint128_t value) {
	return __builtin_bswap128(value);
}
#	endif

#endif
