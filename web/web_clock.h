#pragma once

#include <cstdint>

#include "tama/clock.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace bailey {

class WebClock final : public tama::Clock {
 public:
  uint64_t now_unix_ms() override {
#ifdef __EMSCRIPTEN__
    // Date.now() returns a JS Number which Emscripten coerces to double.
    return (uint64_t)EM_ASM_DOUBLE({ return Date.now(); });
#else
    return 0;
#endif
  }
  bool is_synced() override {
#ifdef __EMSCRIPTEN__
    return true;
#else
    return false;
#endif
  }
};

}  // namespace bailey
