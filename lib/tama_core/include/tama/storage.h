#pragma once

#include "tama/save.h"

namespace tama {

class Storage {
 public:
  virtual ~Storage() = default;
  virtual bool load(SaveData& out) = 0;
  virtual void save(const SaveData& data) = 0;
};

}  // namespace tama
