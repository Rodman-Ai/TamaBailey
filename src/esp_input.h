#pragma once

#include <OneButton.h>

#include "tama/game.h"

namespace bailey {

class EspInput {
 public:
  EspInput(tama::Game& game);
  void begin();
  void tick();

 private:
  static EspInput* instance_;

  static void onClickA();  static void onLongA();
  static void onClickB();  static void onLongB();
  static void onClickC();  static void onLongC();

  tama::Game& game_;
  OneButton btn_a_;
  OneButton btn_b_;
  OneButton btn_c_;
};

}  // namespace bailey
