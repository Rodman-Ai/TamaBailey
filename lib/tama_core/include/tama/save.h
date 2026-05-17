#pragma once

#include <cstddef>
#include <cstdint>

#include "tama/pet.h"

namespace tama {

constexpr uint32_t kSaveMagic   = 0x42414C59u;  // "BALY"
constexpr uint16_t kSaveVersion = 1;
constexpr size_t   kSaveBytes   = 36;

#pragma pack(push, 1)
struct SaveData {
  uint32_t magic;
  uint16_t version;
  uint8_t  hunger;
  uint8_t  happiness;
  uint8_t  cleanliness;
  uint8_t  energy;
  uint8_t  life_stage;
  uint8_t  _pad0;
  uint64_t age_ms;
  uint64_t healthy_streak_ms;
  uint64_t neglect_streak_ms;
};
#pragma pack(pop)

static_assert(sizeof(SaveData) == kSaveBytes, "SaveData size mismatch");

void pet_to_save(const Pet& pet, SaveData& out);
bool save_to_pet(const SaveData& in, Pet& out);

}  // namespace tama
