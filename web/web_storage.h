#pragma once

#include <cstdint>
#include <cstring>

#include "tama/storage.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace bailey {

class WebStorage final : public tama::Storage {
 public:
  bool load(tama::SaveData& out) override {
#ifdef __EMSCRIPTEN__
    int ok = EM_ASM_INT({
      try {
        var s = localStorage.getItem('tama_bailey_save_v1');
        if (!s) return 0;
        var bin = atob(s);
        if (bin.length !== $1) return 0;
        var arr = new Uint8Array(bin.length);
        for (var i = 0; i < bin.length; ++i) arr[i] = bin.charCodeAt(i);
        HEAPU8.set(arr, $0);
        return 1;
      } catch (e) { return 0; }
    }, &out, (int)sizeof(out));
    return ok == 1;
#else
    (void)out;
    return false;
#endif
  }

  void save(const tama::SaveData& data) override {
#ifdef __EMSCRIPTEN__
    EM_ASM({
      try {
        var bytes = new Uint8Array(HEAPU8.buffer, $0, $1);
        var s = '';
        for (var i = 0; i < bytes.length; ++i) s += String.fromCharCode(bytes[i]);
        localStorage.setItem('tama_bailey_save_v1', btoa(s));
      } catch (e) {}
    }, &data, (int)sizeof(data));
#else
    (void)data;
#endif
  }
};

}  // namespace bailey
