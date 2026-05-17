#include "esp_clock.h"

#include <WiFi.h>
#include <time.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace bailey {

namespace {
bool g_synced = false;
unsigned long g_last_try = 0;

bool have_creds() {
#if defined(WIFI_SSID) && defined(WIFI_PASS)
  return WIFI_SSID[0] != 0 && WIFI_SSID[0] != 'y';  // skip the example "your-ssid-here"
#else
  return false;
#endif
}
}  // namespace

void EspClock::begin() {
  if (!have_creds()) {
    log_i("[clock] no Wi-Fi credentials; staying unsynced (game uses synthetic day/night)");
    return;
  }
#if defined(WIFI_SSID) && defined(WIFI_PASS)
  log_i("[clock] connecting Wi-Fi: %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // configTime(gmt_offset_sec, daylight_offset_sec, ntp_server...);
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
#endif
}

void EspClock::poll() {
  if (g_synced) return;
  if (millis() - g_last_try < 2000) return;
  g_last_try = millis();
  if (WiFi.status() != WL_CONNECTED) return;
  time_t now = time(nullptr);
  if (now > 1700000000) {  // past 2023-11-14, definitely synced
    g_synced = true;
    log_i("[clock] NTP synced: %lu", (unsigned long)now);
  }
}

uint64_t EspClock::now_unix_ms() {
  if (!g_synced) return 0;
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) != 0) return 0;
  return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}

bool EspClock::is_synced() { return g_synced; }

}  // namespace bailey
