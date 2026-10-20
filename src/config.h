// --- config.h ---
#pragma once

// ──────────────── RAGE ────────────────
struct RageAimbot_t {
    bool  enabled = false;
    int   key = 0x06;
    bool  silent = true;
    bool  auto_fire = false;
    bool  auto_scope = false;
    int   auto_stop = 0;

    bool  hit_head = true;
    bool  hit_neck = true;
    bool  hit_chest = true;
    bool  hit_stomach = false;
    bool  hit_pelvis = false;
    bool  hit_legs = false;

    int   fov = 180;
    int   max_targets = 1;
};

struct RageAccuracy_t {
    float hit_chance = 65.0f;
    float min_damage = 40.0f;
    float penetration = 0.0f;
    bool  prefer_body = false;
    bool  prefer_safe = false;

    bool  multipoint = false;
    float mp_scale = 50.0f;

    bool  backtrack = false;
    int   backtrack_ticks = 12;
};

struct RageExploits_t {
    bool  double_tap = false;
    bool  hide_shots = false;
    bool  rapid_fire = false;
    int   tickbase = 0;
    int   fake_lag_ms = 0;
};

struct RageConfig_t {
    RageAimbot_t   aimbot;
    RageAccuracy_t accuracy;
    RageExploits_t exploits;
};

// ──────────────── ANTI-AIM ────────────────
struct AntiAimConfig_t {
    bool  enabled = false;
    int   mode = 0;
    int   pitch = 0;
    int   yaw_base = 0;
    int   yaw_add = 0;
    int   jitter_range = 90;
    int   spin_speed = 30;
    bool  fake_yaw = false;
    int   fake_yaw_amount = 58;
    int   desync_side = 0;
    int   on_shot = 0;
};

// ──────────────── VISUALS ────────────────
struct VisualsPlayer_t {
    bool  esp_box = false;
    bool  esp_name = false;
    bool  esp_health = false;
    bool  esp_armor = false;
    bool  esp_weapon = false;
    bool  esp_distance = false;
    bool  esp_skeleton = false;

    float box_color[4] = { 0.92f, 0.28f, 0.28f, 1.0f };
    float box_enemy[4] = { 0.92f, 0.28f, 0.28f, 1.f };
    float box_team[4] = { 0.34f, 0.84f, 0.55f, 1.f };
    float name_color[4] = { 0.96f, 0.96f, 0.96f, 1.0f };
    float skel_color[4] = { 0.24f, 0.55f, 0.83f, 1.0f };
    float weapon_color[4] = { 0.84f, 0.84f, 0.84f, 1.0f };
    float distance_color[4] = { 0.75f, 0.75f, 0.75f, 1.0f };

    bool  third_person = false;
    float third_person_dist = 120.f;

    bool  glow = false;
    float glow_color[4] = { 1.f, 0.25f, 0.25f, 1.f };   // menu reads this
    float glow_enemy[4] = { 1.f, 0.25f, 0.25f, 1.f };
    float glow_team[4] = { 0.25f, 1.f, 0.4f, 1.f };
    bool  glow_weapons = false;
    bool  glow_bomb = false;
    float glow_weapon_color[4] = { 1.f, 0.85f, 0.2f, 1.f };
    float glow_bomb_color[4] = { 1.f, 0.4f, 0.15f, 1.f };

    bool  chams = false;
    bool  chams_team = false;
    int   chams_mat = 0;                                // menu combo
    float chams_vis[4] = { 1.f, 0.3f, 0.3f, 1.f };      // menu color
    float chams_occ[4] = { 0.3f, 0.6f, 1.f, 1.f };      // menu color
    float chams_visible[4] = { 1.f, 0.3f, 0.3f, 1.f };
    float chams_hidden[4] = { 0.3f, 0.6f, 1.f, 1.f };
};

struct VisualsWorld_t {
    bool  radar = false;
    int   radar_pos = 0;
    int   radar_zoom = 20;

    bool  dropped_weapons = false;
    bool  grenade_pred = false;
    bool  bomb_esp = false;
    bool  bomb_timer = false;
    bool  damage_markers = false;
    bool  no_scope = false;
};

struct VisualsConfig_t {
    VisualsPlayer_t player;
    VisualsWorld_t  world;
};

// ──────────────── MISC ────────────────
struct MiscMovement_t {
    bool  bhop = false;
    bool  auto_strafe = false;
    bool  fast_stop = false;
};

struct MiscWorld_t {
    bool  no_flash = false;
    bool  no_smoke = false;
    bool  reveal_ranks = false;
};

struct MiscExploits_t {
    bool  anti_untrusted = false;
    bool  clantag = false;
    char  clantag_text[32] = "vanta";
    bool  chat_spam = false;
    char  chat_text[128] = "";
};

struct MiscInput_t {
    int   menu_key = 0x2D;
    int   unload_key = 0x23;
};

struct MiscConfig_t {
    MiscMovement_t movement;
    MiscWorld_t    world;
    MiscExploits_t exploits;
    MiscInput_t    input;
};

// ──────────────── UI ────────────────
struct UIConfig_t {
    int   bg_style = 1;
    float bg_intensity = 1.0f;
    float bg_speed = 1.0f;
    float window_alpha = 0.88f;
    int   theme_preset = 0;
    float accent_custom[4] = { 0.24f, 0.55f, 0.83f, 1.0f };
};

// ──────────────── ROOT ────────────────
struct Config_t {
    RageConfig_t    rage;
    AntiAimConfig_t anti_aim;
    VisualsConfig_t visuals;
    MiscConfig_t    misc;
    UIConfig_t      ui;
};

inline Config_t g_cfg;