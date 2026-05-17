#pragma once

#include <Preferences.h>

#include "tama/storage.h"

namespace bailey {

class EspStorage final : public tama::Storage {
 public:
  bool load(tama::SaveData& out) override;
  void save(const tama::SaveData& data) override;
};

}  // namespace bailey
