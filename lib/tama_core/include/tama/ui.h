#pragma once

#include <cstdint>

#include "tama/renderer.h"

namespace tama {

class Game;  // forward decl -- ui.cpp consumes Game for full state access

constexpr int kScreenW = 240;
constexpr int kScreenH = 240;

constexpr int kStatsBarH = 32;
constexpr int kStatusH   = 36;

bool point_on_pet(int x, int y);
bool point_on_stats_bar(int x, int y);

// Top-level rendering. Pulls everything it needs (pet, settings, scene,
// weather, daylight, clock string, etc.) from `game`.
void draw_scene(Renderer& r, const Game& game, uint32_t now_ms);

void draw_menu_overlay(Renderer& r, const Game& game);

}  // namespace tama
