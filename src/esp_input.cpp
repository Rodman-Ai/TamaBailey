#include "esp_input.h"

#include "pins.h"

namespace bailey {

EspInput* EspInput::instance_ = nullptr;

// OneButton(pin, activeLow=true, pullupActive=true): use the internal pull-up
// and treat LOW as pressed -- matches the Waveshare buttons (switch-to-GND).
EspInput::EspInput(tama::Game& game)
    : game_(game),
      btn_a_(PIN_BTN_A, true, true),
      btn_b_(PIN_BTN_B, true, true),
      btn_c_(PIN_BTN_C, true, true) {
  instance_ = this;
}

void EspInput::begin() {
  btn_a_.attachClick(onClickA);
  btn_b_.attachClick(onClickB);
  btn_c_.attachClick(onClickC);
  btn_a_.attachLongPressStart(onLongA);
  btn_b_.attachLongPressStart(onLongB);
  btn_c_.attachLongPressStart(onLongC);
  btn_a_.setPressMs(800);
  btn_b_.setPressMs(800);
  btn_c_.setPressMs(800);
}

void EspInput::tick() {
  btn_a_.tick();
  btn_b_.tick();
  btn_c_.tick();
}

void EspInput::onClickA() { if (instance_) instance_->game_.enqueue(tama::Input::Feed); }
void EspInput::onClickB() { if (instance_) instance_->game_.enqueue(tama::Input::Play); }
void EspInput::onClickC() { if (instance_) instance_->game_.enqueue(tama::Input::Clean); }

void EspInput::onLongA() {
  if (!instance_) return;
  if (instance_->game_.pet().stage == tama::LifeStage::Gone)
    instance_->game_.enqueue(tama::Input::Restart);
  else
    instance_->game_.enqueue(tama::Input::MenuToggle);
}
void EspInput::onLongB() { onLongA(); }
void EspInput::onLongC() { onLongA(); }

}  // namespace bailey
