#pragma once

#include <cstdint>

namespace tama {

// Wall-clock abstraction. The core only needs to know "what time is it"
// and whether the clock is trustworthy (synced from NTP or similar).
//
// Coordinates: unix epoch ms (UTC). Local time is computed by adding
// `tz_offset_min * 60000` when displaying.
class Clock {
 public:
  virtual ~Clock() = default;
  virtual uint64_t now_unix_ms() = 0;
  virtual bool     is_synced()   = 0;
};

// Helpers usable everywhere.
struct LocalTime {
  int year;
  int month;     // 1..12
  int day;       // 1..31
  int hour;      // 0..23
  int minute;    // 0..59
  int second;    // 0..59
  int day_of_year; // 1..366
};

LocalTime to_local(uint64_t unix_ms, int tz_offset_min);

// Returns 0.0 at midnight, 1.0 at end-of-day for the given local time.
float day_fraction(const LocalTime& lt);

// 24h day index since unix epoch, in the given tz. Used for streak math.
uint32_t local_day_index(uint64_t unix_ms, int tz_offset_min);

}  // namespace tama
