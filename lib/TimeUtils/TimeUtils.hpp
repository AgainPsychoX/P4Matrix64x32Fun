#pragma once

#include <cstdint>
#include <type_traits>
#include <limits>
#include <tuple>



////////////////////////////////////////////////////////////////////////////////

enum class Month : uint8_t {
	January   = 1,
	February  = 2,
	March     = 3,
	April     = 4,
	May       = 5,
	June      = 6,
	July      = 7,
	August    = 8,
	September = 9,
	October   = 10,
	November  = 11,
	December  = 12,
};

enum class DayOfWeek : uint8_t {
	Monday      = 0,
	Tuesday     = 1,
	Wednesday   = 2,
	Thursday    = 3,
	Friday      = 4,
	Saturday    = 5,
	Sunday      = 6, 
};

////////////////////////////////////////////////////////////////////////////////

/// Returns number of days since civil 1970-01-01. 
/// Negative values indicate days prior to 1970-01-01.
/// Preconditions:
/// * y-m-d represents a date in the civil (Gregorian) calendar
/// * m is in [1, 12]
/// * d is in [1, last_day_of_month(y, m)]
/// Adapted from https://stackoverflow.com/a/32158604/4880243
constexpr int days_from_civil(int y, unsigned m, unsigned d) noexcept
{
	static_assert(std::numeric_limits<unsigned>::digits >= 18,
			"This algorithm has not been ported to a 16 bit unsigned integer");
	static_assert(std::numeric_limits<int>::digits >= 20,
			"This algorithm has not been ported to a 16 bit signed integer");
	y -= m <= 2;
	const int era = (y >= 0 ? y : y-399) / 400;
	const unsigned yoe = static_cast<unsigned>(y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;
	return era * 146097 + doe - 719468;
}
constexpr int days_from_civil(int y, Month m, unsigned d) noexcept
{
	return days_from_civil(y, static_cast<uint8_t>(m), d);
}

/// Returns year/month/day triple in civil calendar, given number of days since 1970-01-01.
/// Adapted from https://stackoverflow.com/a/32158604/4880243
constexpr std::tuple<int, unsigned, unsigned> civil_from_days(int z) noexcept
{
	static_assert(std::numeric_limits<unsigned>::digits >= 18,
			"This algorithm has not been ported to a 16 bit unsigned integer");
	static_assert(std::numeric_limits<int>::digits >= 20,
			"This algorithm has not been ported to a 16 bit signed integer");
	z += 719468;
	const int era = (z >= 0 ? z : z - 146096) / 146097;
	const unsigned doe = static_cast<unsigned>(z - era * 146097);
	const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int y = static_cast<int>(yoe) + era * 400;
	const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const unsigned mp = (5 * doy + 2) / 153;
	const unsigned d = doy - (153 * mp + 2) / 5 + 1;
	const unsigned m = mp + (mp < 10 ? 3 : -9);
	return std::tuple<int, unsigned, unsigned>(y + (m <= 2), m, d);
}

/// Returns day of week in civil calendar [0, 6] -> [Sun, Sat].
/// Adapted from https://stackoverflow.com/a/32158604/4880243
constexpr unsigned weekday_from_days(int z) noexcept
{
	return static_cast<unsigned>(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6);
}

////////////////////////////////////////////////////////////////////////////////

struct Duration
{
	uint32_t totalSeconds;

	inline bool operator == (const Duration& other) { return this->totalSeconds == other.totalSeconds; }
	inline bool operator != (const Duration& other) { return this->totalSeconds != other.totalSeconds; }
	inline bool operator <  (const Duration& other) { return this->totalSeconds <  other.totalSeconds; }
	inline bool operator >  (const Duration& other) { return this->totalSeconds >  other.totalSeconds; }
	inline bool operator <= (const Duration& other) { return this->totalSeconds <= other.totalSeconds; }
	inline bool operator >= (const Duration& other) { return this->totalSeconds >= other.totalSeconds; }
};

////////////////////////////////////////////////////////////////////////////////

/// Represents time of day in given environment.
struct TimeOfDay
{
	uint8_t second;
	uint8_t minute;
	uint8_t hour;

	static constexpr TimeOfDay fromUnixMillis(uint64_t value) noexcept {
		uint32_t r = value / 1000; // ignore milliseconds
		const uint8_t second = r % 60; r /= 60;
		const uint8_t minute = r % 60; r /= 60;
		const uint8_t hour   = r % 24;
		return {second, minute, hour};
	}

	constexpr bool isValid() const noexcept {
		if (60 <= second) return false;
		if (60 <= minute) return false;
		if (24 <= hour) return false;
		return true;
	}
};
static_assert(sizeof(TimeOfDay) <= 4);

constexpr int dayAsSeconds = 24 * 60 * 60;
constexpr int dayAsMillis = dayAsSeconds * 1000;

////////////////////////////////////////////////////////////////////////////////

struct Date
{
	uint8_t day;
	Month month;
	int16_t year;

	static constexpr Date fromUnixMillis(uint64_t value) noexcept {
		const auto [year, month, day] = civil_from_days(value / dayAsMillis);
		return {static_cast<uint8_t>(day), static_cast<Month>(month), static_cast<int16_t>(year)};
	}

	constexpr DayOfWeek dayOfWeek() const noexcept {
		return static_cast<DayOfWeek>(weekday_from_days(days_from_civil(year, month, day)));
	}

	constexpr bool isValid() const noexcept {
		// TODO: check days in month, leap years too
		return true;
	}
};
static_assert(sizeof(Date) <= 4);

////////////////////////////////////////////////////////////////////////////////

struct DateTime : public TimeOfDay, public Date
{
	constexpr DateTime() : TimeOfDay{}, Date{} {}

	constexpr DateTime(
		uint8_t second, uint8_t minute, uint8_t hour,
		uint8_t day, Month month, int16_t year
	) : 
		TimeOfDay{second, minute, hour},
		Date{day, month, year}
	{}
	
	constexpr DateTime(
		uint8_t second, uint8_t minute, uint8_t hour,
		uint8_t day, uint8_t month, int16_t year
	) : 
		TimeOfDay{second, minute, hour},
		Date{day, static_cast<Month>(month), year}
	{}
	
	constexpr uint32_t toUnixSeconds() const noexcept {
		const int days = days_from_civil(year, month, day);
		return (((days * 24 + hour) * 60 + minute) * 60 + second);
	}

	/// Time as number of microseconds since Unix epoch (Jan 1 1970).
	constexpr uint64_t toUnixMillis() const noexcept {
		return toUnixSeconds() * 1000;
	}

	static constexpr DateTime fromUnixMillis(uint64_t value) noexcept {
		uint32_t r = value / 1000; // ignore milliseconds
		const uint8_t second = r % 60; r /= 60;
		const uint8_t minute = r % 60; r /= 60;
		const uint8_t hour   = r % 24; r /= 24;
		const auto [year, month, day] = civil_from_days(r);
		return {second, minute, hour, static_cast<uint8_t>(day), static_cast<uint8_t>(month), static_cast<int16_t>(year)};
	}

	constexpr Duration operator - (const DateTime& other) const noexcept {
		const auto thisDays  = days_from_civil(this->year, this->month, this->day);
		const auto otherDays = days_from_civil(other.year, other.month, other.day);
		const auto seconds = std::abs(
			(((thisDays - otherDays) * 60 + 
				this->hour - other.hour) * 60 + 
					this->minute - other.minute) * 60 + 
						this->second - other.second
		);
		return {static_cast<uint32_t>(seconds)};
	}

	constexpr bool isValid() const noexcept {
		return this->TimeOfDay::isValid() && this->Date::isValid();
	}
};
static_assert(sizeof(DateTime) <= 8);

////////////////////////////////////////////////////////////////////////////////


