#include "esp_storage.h"

namespace bailey {

namespace {
constexpr const char* kNs  = "bailey";
constexpr const char* kKey = "state";
}

bool EspStorage::load(tama::SaveData& out) {
  Preferences p;
  if (!p.begin(kNs, true)) return false;
  size_t n = p.getBytesLength(kKey);
  if (n != sizeof(tama::SaveData)) { p.end(); return false; }
  size_t read = p.getBytes(kKey, &out, sizeof(out));
  p.end();
  return read == sizeof(out);
}

void EspStorage::save(const tama::SaveData& data) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBytes(kKey, &data, sizeof(data));
  p.end();
}

}  // namespace bailey
