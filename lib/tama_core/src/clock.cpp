#include "tama/clock.h"

namespace tama {

// Days in each month of a non-leap year.
static const int kDaysInMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

static bool is_leap(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

LocalTime to_local(uint64_t unix_ms, int tz_offset_min) {
  int64_t adjusted = (int64_t)(unix_ms / 1000) + (int64_t)tz_offset_min * 60;
  if (adjusted < 0) adjusted = 0;
  uint64_t s = (uint64_t)adjusted;

  int second = (int)(s % 60); s /= 60;
  int minute = (int)(s % 60); s /= 60;
  int hour   = (int)(s % 24); s /= 24;
  uint64_t days = s;

  int year = 1970;
  while (true) {
    int dpy = is_leap(year) ? 366 : 365;
    if ((int)days < dpy) break;
    days -= dpy;
    ++year;
  }
  int month = 0;
  int day_of_year = (int)days + 1;
  while (month < 12) {
    int dim = kDaysInMonth[month] + ((month == 1 && is_leap(year)) ? 1 : 0);
    if ((int)days < dim) break;
    days -= dim;
    ++month;
  }

  LocalTime lt{};
  lt.year = year;
  lt.month = month + 1;
  lt.day   = (int)days + 1;
  lt.hour = hour;
  lt.minute = minute;
  lt.second = second;
  lt.day_of_year = day_of_year;
  return lt;
}

float day_fraction(const LocalTime& lt) {
  int s = lt.hour * 3600 + lt.minute * 60 + lt.second;
  return (float)s / 86400.0f;
}

uint32_t local_day_index(uint64_t unix_ms, int tz_offset_min) {
  int64_t adjusted = (int64_t)(unix_ms / 1000) + (int64_t)tz_offset_min * 60;
  if (adjusted < 0) adjusted = 0;
  return (uint32_t)((uint64_t)adjusted / 86400ULL);
}

}  // namespace tama
