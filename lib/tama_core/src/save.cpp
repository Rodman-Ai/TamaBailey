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
  if (s.version < 8) {
    // v7 -> v8: zero the daily-quest reward tracker.
    s.daily_quest_awarded_day = 0;
  }
  if (s.version < 9) {
    // v8 -> v9: no bonded best friend yet.
    s.best_friend_hash = 0;
  }
  if (s.version < 10) {
    // v9 -> v10: no gifts redeemed yet.
    s.last_gift_received_day = 0;
  }
  if (s.version < 11) {
    // v10 -> v11: no bedtime stories yet.
    s.stories_heard = 0;
    s._pad11        = 0;
  }
  if (s.version < 12) {
    // v11 -> v12: no successful walk-digs yet.
    s.dig_successes = 0;
    s._pad12        = 0;
  }
  if (s.version < 13) {
    // v12 -> v13: no seasonal accessories unlocked yet.
    s.seasonal_unlocks = 0;
    s._pad13[0] = s._pad13[1] = s._pad13[2] = 0;
  }
  if (s.version < 14) {
    // v13 -> v14: no bath toys yet.
    s.bath_toys_owned = 0;
    s.bath_toy_active = 0;
    s._pad14[0] = s._pad14[1] = 0;
  }
  if (s.version < 15) {
    // v14 -> v15: no hide & seek wins yet.
    s.hide_seek_wins = 0;
    s._pad15         = 0;
  }
  if (s.version < 16) {
    // v15 -> v16: widen achievements_ to 64 bits by zero-init'ing
    // the new high word; the low 32 bits are already populated.
    s.achievements_hi = 0;
    s._pad16          = 0;
  }
  if (s.version < 17) {
    // v16 -> v17: seed default pet name + birthday.
    std::memcpy(s.pet_name, "Bailey", 7);   // includes null terminator
    s.birthday_month = 1;
    s.birthday_day   = 13;
    s._pad17[0] = s._pad17[1] = 0;
  }
  if (s.version < 18) {
    // v17 -> v18: zero new decor + sticker state.
    s.stickers_unlocked = 0;
    s.wall_poster       = 0;
    s._pad18[0] = s._pad18[1] = 0;
  }
  if (s.version < 19) {
    // v18 -> v19: zero progression counters.
    s.trainer_xp     = 0;
    s.time_played_ms = 0;
  }
  if (s.version < 20) {
    // v19 -> v20: zero active-streak counter.
    s.active_streak_days = 0;
    s._pad20             = 0;
  }
  if (s.version < 21) {
    // v20 -> v21: zero firefly counter.
    s.fireflies_caught = 0;
    s._pad21           = 0;
  }
  if (s.version < 22) {
    // v21 -> v22: never-spun login wheel.
    s.last_login_wheel_day = 0;
    s.last_wheel_reward    = 0;
    s._pad22[0] = s._pad22[1] = s._pad22[2] = 0;
  }
  if (s.version < 23) {
    // v22 -> v23: no postcards received yet.
    s.last_postcard_msg_id = 0xFF;       // sentinel = "never received"
    s.postcards_received   = 0;
    s._pad23               = 0;
  }
  if (s.version < 24) {
    // v23 -> v24: default bed (basket) + bowl (blue).
    s.bed_type   = 0;
    s.bowl_color = 0;
    s._pad24     = 0;
  }
  if (s.version < 25) {
    // v24 -> v25: zero all mini-game scores.
    s.fish_caught    = 0;
    s.tug_high_score = 0;
    s.memory_iq      = 0;
    s.vet_visits     = 0;
    s.stick_chases   = 0;
  }
  if (s.version < 26) {
    // v25 -> v26: full health, neutral weight.
    s.health_stat = 100;
    s.pet_weight  = 50;
    s._pad26      = 0;
  }
  if (s.version < 27) {
    // v26 -> v27: no friend bonds, no earned titles, auto-XP title.
    for (int i = 0; i < 8; ++i) s.friend_bond_levels[i] = 0;
    s.earned_titles_mask = 0;
    s.chosen_title_id    = 0;
    s._pad27             = 0;
  }
  if (s.version < 28) {
    // v27 -> v28: empty diary, never-claimed cherry blossom.
    for (int i = 0; i < 7; ++i) s.diary_entries[i] = 0xFF;
    s.diary_head              = 0;
    s.cherry_blossom_last_day = 0;
    s._pad28[0] = s._pad28[1] = s._pad28[2] = 0;
  }
  if (s.version < 29) {
    // v28 -> v29: empty vet history, no auto-feeder owned.
    for (int i = 0; i < 5; ++i) s.vet_history_days[i] = 0;
    s.vet_history_head  = 0;
    s.vet_history_count = 0;
    s.auto_feeder_owned = 0;
    s._pad29            = 0;
  }
  if (s.version < 30) {
    // v29 -> v30: no soul bond, empty wishlist, never-visited timestamps.
    s.soul_bond_friend_id  = 0xFF;
    s.friend_wishlist_mask = 0;
    for (int i = 0; i < 8; ++i) s.friend_last_visit_day[i] = 0;
    s._pad30               = 0;
  }
  if (s.version < 31) {
    // v30 -> v31: empty quest history, no Day of Dogs / cake yet.
    for (int i = 0; i < 7; ++i) s.quest_history[i] = 0xFF;
    s.quest_history_head      = 0;
    s.quest_history_count     = 0;
    s.day_of_dogs_last_day    = 0;
    s.birthday_cake_seen_day  = 0;
    s._pad31[0] = s._pad31[1] = 0;
  }
  if (s.version < 32) {
    // v31 -> v32: no badge, default accessory size, no extra coats.
    s.collar_badge_id      = 0;
    s.accessory_size       = 1;
    s.extra_coats_unlocked = 0;
    s._pad32               = 0;
  }
  if (s.version < 33) {
    // v32 -> v33: no gifts received, zero weekly progress, no perks.
    for (int i = 0; i < 8; ++i) s.friend_last_gift[i] = 0;
    s.weekly_steps_progress      = 0;
    s.weekly_last_awarded_week   = 0;
    s.trainer_perks_mask         = 0;
    s._pad33[0] = s._pad33[1] = s._pad33[2] = 0;
  }
  if (s.version < 34) {
    // v33 -> v34: no daily seals, no halloween costumes.
    s.daily_seals_total            = 0;
    s.daily_seals_last_day         = 0;
    s.halloween_costumes_unlocked  = 0;
    s._pad34[0] = s._pad34[1]      = 0;
  }
  if (s.version < 35) {
    // v34 -> v35: empty trainer leaderboard.
    for (int i = 0; i < 8; ++i) s.leaderboard_hashes[i] = 0;
    s.leaderboard_head        = 0;
    s.leaderboard_count       = 0;
    s._pad35[0] = s._pad35[1] = 0;
  }
  if (s.version < 36) {
    // v35 -> v36: default wallpaper 0, zero mini-game records.
    for (int i = 0; i < 8; ++i) s.scene_wallpaper[i] = 0;
    s.pumpkin_tap_high_score = 0;
    s.trick_chain_runs       = 0;
    s._pad36                 = 0;
  }
  s.version = kSaveVersion;
  return true;
}

}  // namespace tama
