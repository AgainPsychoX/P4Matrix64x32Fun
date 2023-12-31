#pragma once

#include <cstdint>
#include <new>
#include <Arduino.h>
#include <ESP8266WiFi.h> // ip4_addr_t
#include <coredecls.h> // crc32

static_assert(sizeof(int*) == 4, 
	"It's ESP/xtensa AVR, 32 bit project, the pointer should be 4 bytes long. "
	"If you are using VS Code, see https://github.com/platformio/platformio-vscode-ide/issues/3284 related issues. "
);

/// Type for number of milliseconds across Arduino functions like `millis()` or `delay(ms)`
using millis_t = decltype(millis()); 

#include "utils.hpp"

#define LEVEL_NONE  0
#define LEVEL_ERROR 1
#define LEVEL_WARN  2
#define LEVEL_INFO  3
#define LEVEL_DEBUG 4
#define LEVEL_TRACE 5

// FNV1a 32 (source: https://gist.github.com/Lee-R/3839813, license: Public Domain)
constexpr uint32_t fnv1a_32_recursive(const char* const s, size_t count) {
	return count ? (fnv1a_32_recursive(s, count - 1) ^ s[count - 1]) * 16777619u : 2166136261u;
}

template <size_t N>
constexpr uint32_t hashLogCompStr(const char (&str) [N]) {
	return fnv1a_32_recursive(str, N - 1);
}

#define USE_LOG_LEVEL_DEFAULT(level)    template <uint64_t str_hash> struct LogLevelForComponent { static constexpr int value = level; };
#define USE_LOG_LEVEL(component, level) template <> struct LogLevelForComponent<hashLogCompStr(STRINGIFY(component))> { static constexpr int value = level; }
#define GET_LOG_LEVEL(component) LogLevelForComponent<hashLogCompStr(STRINGIFY(component))>::value
#define CHECK_LOG_LEVEL(component, level) (GET_LOG_LEVEL(component) >= level)

#include "config.hpp"

#ifdef DEBUG

// Shared PROGMEM (sub)strings could be reused, but for things as short as 
// component name up to around 16 it isn't worth... Calling `Serial.println` 
// takes 12-16 bytes. Could be lowered to 8 possibly, but still not worth.
// See https://godbolt.org/z/nEKhEqYhf

#define _LOG_IMPL_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, N,...) N
#define _LOG_IMPL(...) _LOG_IMPL_GET_MACRO(__VA_ARGS__, LOG_V, LOG_V, LOG_V, LOG_V, LOG_V, LOG_V, LOG_V, LOG_V, LOG_V, LOG_3, LOG_2)(__VA_ARGS__)

#define LOG_2(level, msg) Serial.println(F(msg));
#define LOG_3(level, comp, msg) Serial.println(F("[" STRINGIFY(comp) "] " msg));
#define LOG_V(level, comp, format, ...) Serial.printf_P(PSTR("[" STRINGIFY(comp) "] " format "\n"), __VA_ARGS__);

#define WHERE_AM_I_STR __FILE__ ":" STRINGIFY(__LINE__)

#	define LOG_ERROR(comp, ...) if (CHECK_LOG_LEVEL(comp, LEVEL_ERROR)) _LOG_IMPL(LEVEL_ERROR, comp, __VA_ARGS__)
#	define LOG_WARN(comp, ...)  if (CHECK_LOG_LEVEL(comp, LEVEL_WARN))  _LOG_IMPL(LEVEL_WARN, comp, __VA_ARGS__)
#	define LOG_INFO(comp, ...)  if (CHECK_LOG_LEVEL(comp, LEVEL_INFO))  _LOG_IMPL(LEVEL_INFO, comp, __VA_ARGS__)
#	define LOG_DEBUG(comp, ...) if (CHECK_LOG_LEVEL(comp, LEVEL_DEBUG)) _LOG_IMPL(LEVEL_DEBUG, comp, __VA_ARGS__)
#	define LOG_TRACE(comp, ...) if (CHECK_LOG_LEVEL(comp, LEVEL_TRACE)) _LOG_IMPL(LEVEL_TRACE, comp, __VA_ARGS__)

#endif

#ifndef LOG_ERROR
#	define LOG_ERROR(...) 
#endif
#ifndef LOG_WARN
#	define LOG_WARN(...) 
#endif
#ifndef LOG_INFO
#	define LOG_INFO(...) 
#endif
#ifndef LOG_DEBUG
#	define LOG_DEBUG(...) 
#endif
#ifndef LOG_TRACE
#	define LOG_TRACE(...) 
#endif



////////////////////////////////////////////////////////////////////////////////
// Some global externs for misc stuff

#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "webEncoded/WebCommonUtils.hpp"

// Initialized in main
extern ESP8266WebServer webServer;

// Get settings object (which is persisted in EEPROM)
extern Settings* settings;

// Util to keep track of used RAM, from main
extern std::ptrdiff_t getStackOffsetFromSetup();


