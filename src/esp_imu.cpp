#include "esp_imu.h"

#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>

#include "pins.h"

namespace bailey {

namespace {
SensorQMI8658 g_qmi;
IMUdata       g_acc;
}

bool EspImu::begin() {
  Wire.begin(PIN_IMU_SDA, PIN_IMU_SCL, 400000);
  if (!g_qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_IMU_SDA, PIN_IMU_SCL)) {
    Serial.println("[imu] QMI8658 not detected -- gestures disabled");
    ok_ = false;
    return false;
  }
  g_qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                            SensorQMI8658::ACC_ODR_250Hz,
                            SensorQMI8658::LPF_MODE_0);
  g_qmi.enableAccelerometer();
  ok_ = true;
  Serial.printf("[imu] QMI8658 detected, chip id 0x%02X\n",
                g_qmi.getChipID());
  return true;
}

void EspImu::poll(uint32_t now_ms, tama::Game& game) {
  if (!ok_) return;
  if (now_ms - last_poll_ms_ < 20) return;   // ~50 Hz
  last_poll_ms_ = now_ms;
  if (!g_qmi.getDataReady()) return;
  if (!g_qmi.getAccelerometer(g_acc.x, g_acc.y, g_acc.z)) return;

  float ax = g_acc.x;   // units are g (1.0 ~= 9.8 m/s^2 at rest gravity component)
  float ay = g_acc.y;
  float az = g_acc.z;

  // ---- Shake detection: rapid X-axis sign reversal with high peak ----
  int8_t sign = (ax > 0.4f) ? 1 : (ax < -0.4f) ? -1 : 0;
  recent_sign_[recent_idx_] = sign;
  recent_idx_ = (uint8_t)((recent_idx_ + 1) % 8);
  int reversals = 0;
  for (int i = 1; i < 8; ++i) {
    int a = recent_sign_[i];
    int b = recent_sign_[i - 1];
    if (a != 0 && b != 0 && a != b) ++reversals;
  }
  if (reversals >= 4 && (ax > 2.0f || ax < -2.0f) &&
      now_ms - last_shake_ms_ > 600) {
    last_shake_ms_ = now_ms;
    Serial.println("[imu] shake detected -> MicTrigger");
    game.enqueue(tama::Input::MicTrigger);
  }

  // ---- Flick detection: large forward (+X) accel spike ----
  // ax measured at rest is ~0; a quick wrist flick spikes well above 2 G.
  if (ax > 2.5f && now_ms - last_flick_ms_ > 400) {
    last_flick_ms_ = now_ms;
    Serial.println("[imu] flick detected -> ImuFlick");
    game.enqueue(tama::Input::ImuFlick);
  }

  // ---- Step detection: vertical (Z) pulse edge ----
  // At rest Z ~ 1 G. A walking step bobs Z to ~1.4-1.8 G then back.
  if (!step_high_ && az > 1.4f) {
    step_high_ = true;
    if (now_ms - last_step_ms_ > 350) {
      last_step_ms_ = now_ms;
      Serial.println("[imu] step detected -> Walk");
      game.enqueue(tama::Input::Walk);
    }
  } else if (step_high_ && az < 1.1f) {
    step_high_ = false;
  }
}

}  // namespace bailey
