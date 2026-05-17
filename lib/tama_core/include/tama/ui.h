#pragma once

#include <cstdint>

#include "tama/pet.h"
#include "tama/renderer.h"

namespace tama {

constexpr int kScreenW = 240;
constexpr int kScreenH = 240;

// Top stats bar height + bottom status area.
constexpr int kStatsBarH = 32;
constexpr int kStatusH   = 36;

// Returns true if (x, y) lands on the pet sprite's bounding box (used for
// optional touch input).
bool point_on_pet(int x, int y);
bool point_on_stats_bar(int x, int y);

// Top-level scene draw (background, stats bar, pet+accessories, footer).
void draw_scene(Renderer& r, const Pet& pet, uint32_t now_ms);

// Modal overlay with numeric stats / age / streaks.
void draw_menu_overlay(Renderer& r, const Pet& pet);

}  // namespace tama
