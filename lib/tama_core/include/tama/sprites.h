#pragma once

#include <cstdint>

#include "tama/pet.h"

namespace tama {

constexpr int kPetSpriteSize = 48;  // square
constexpr int kAccessorySize = 16;

enum class PetPose : uint8_t {
  IdleA   = 0,
  IdleB   = 1,
  Eating  = 2,
  Playing = 3,
  Sleep   = 4,
  Sad     = 5,
  Gone    = 6,
  // Round 3: ambient behaviors
  Sit     = 7,
  Bark    = 8,
  Pant    = 9,
  COUNT   = 10,
};

// Generate all sprite buffers. Must be called once before drawing.
void sprites_init();

// Get the 48x48 indexed-color buffer for a given pet pose & life stage.
const uint8_t* pet_sprite(LifeStage stage, PetPose pose);

// Small accessories drawn next to / above the pet.
const uint8_t* food_bowl_sprite();      // 16x16
const uint8_t* ball_sprite();           // 16x16
const uint8_t* poop_sprite();           // 16x16
const uint8_t* bubble_sprite();         // 16x16
const uint8_t* zzz_sprite();            // 16x16
const uint8_t* heart_sprite();          // 16x16

}  // namespace tama
