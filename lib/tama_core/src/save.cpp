#include "tama/save.h"

#include <cstring>

namespace tama {

bool save_validate_and_migrate(SaveData& s) {
  if (s.magic != kSaveMagic)                 return false;
  if (s.version > kSaveVersion)              return false;  // unknown future
  if (s.life_stage > (uint8_t)LifeStage::Gone) return false;

  if (s.version < 2) {
    // v1 -> v2: zero everything past the base block.
    s.settings                  = default_settings();
    s.achievements              = 0;
    s.streak_days               = 0;
    s._pad1                     = 0;
    s.streak_last_visit_unix_ms = 0;
    s.last_save_real_unix_ms    = 0;
    s.total_pets                = 0;
    s.fetch_catches             = 0;
    s.coat_pattern              = 0;
    s.accessory_id              = 0;
    s.personality_trait         = 0;
    s.inherited_trait           = 0;
    s.tricks_learned            = 0;
    s.weather                   = 0;
    s.sickness                  = 0;
    s.scene_id                  = 0;
    std::memset(s.memorial, 0, sizeof(s.memorial));
    s.memorial_count            = 0;
    s.memorial_head             = 0;
    std::memset(s._pad2, 0, sizeof(s._pad2));
  }
  if (s.version < 3) {
    // v2 -> v3: zero the Round-2 additions.
    s.biscuits                       = 0;
    s.toy_owned                      = 1;  // ball unlocked by default
    s.active_toy                     = 0;
    s.treats[0] = s.treats[1] = s.treats[2] = 0;
    s.wish                           = 0;
    s.wish_started_ms                = 0;
    s.birthday_celebrated_unix_day   = 0;
    s.well_tucked_in_today           = 0;
    s.vocab_learned                  = 0;
    for (int i = 0; i < 5; ++i) s.trick_perf[i] = 0;
    s.total_steps                    = 0;
    std::memset(s.mood_history, 0, sizeof(s.mood_history));
    s.mood_history_head              = 0;
    std::memset(s._pad3, 0, sizeof(s._pad3));
  }
  if (s.version < 4) {
    // v3 -> v4: zero the original four friend_visits_[] slots.
    for (int i = 0; i < 4; ++i) s.friend_visits[i] = 0;
  }
  if (s.version < 5) {
    // v4 -> v5: zero the new Ruben slot.
    s.friend_visits[4] = 0;
  }
  if (s.version < 6) {
    // v5 -> v6: zero the new Francie / Bomi / Noshy slots.
    s.friend_visits[5] = 0;
    s.friend_visits[6] = 0;
    s.friend_visits[7] = 0;
  }
  if (s.version < 7) {
    // v6 -> v7: zero the Round-3 walk-collectible fields.
    s.bones_collected   = 0;
    s.walk_today_steps  = 0;
    s._pad7             = 0;
  }
  s.version = kSaveVersion;
  return true;
}

}  // namespace tama
