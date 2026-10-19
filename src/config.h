// language: C++, file: config.h
// *Config global completo. Los widgets escriben aquí, el rage lo lee desde CreateMove.*

#pragma once

// ──────────────── RAGE ────────────────
struct RageAimbot_t {
    bool  enabled = false;
    int   key = 0x06;   // mouse5 por defecto
    bool  silent = true;
    bool  auto_fire = false;
    bool  auto_scope = false;
    int   auto_stop = 0;      // 0=off 1=slow 2=full

    // hitbox selection — 6 bools
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
    int   tickbase = 0;      // 0=off 1=teleport 2=charge 3=fake lag
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
    int   mode = 0;      // 0=jitter 1=spin 2=static 3=random
    int   pitch = 0;      // 0=down 1=up 2=zero 3=fake
    int   yaw_base = 0;      // -180..180
    int   yaw_add = 0;      // -180..180
    int   jitter_range = 90;     // 0..180
    int   spin_speed = 30;     // 1..90
    bool  fake_yaw = false;
    int   fake_yaw_amount = 58;     // 0..60
    int   desync_side = 0;      // 0=auto 1=left 2=right
    int   on_shot = 0;      // 0=off 1=fake 2=reverse
};

// ──────────────── VISUALS ────────────────
struct VisualsPlayer_t {
    bool  esp_box = false;
    bool  esp_name = false;
    bool  esp_health = false;
    bool  esp_weapon = false;
    bool  esp_skeleton = false;
    float skel_color[4] = { 0.24f, 0.55f, 0.83f, 1.0f };
    bool  chams = false;
    int   chams_mat = 0;
    float chams_vis[4] = { 0.24f, 0.55f, 0.83f, 1.0f };
    float chams_occ[4] = { 0.85f, 0.05f, 0.03f, 1.0f };
    bool  glow = false;
    float glow_color[4] = { 0.85f, 0.05f, 0.03f, 1.0f };

    // ── camera ──
    bool  third_person = false;
    float third_person_dist = 150.0f;
};

struct VisualsWorld_t {
    bool  radar = false;
    int   radar_pos = 0;      // 0=left 1=right 2=center
    int   radar_zoom = 20;     // 0..100
    bool  dropped_weapons = false;
    bool  grenade_pred = false;
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
    int   menu_key = 0x2D;   // Insert
    int   unload_key = 0x23;   // End
};

struct MiscConfig_t {
    MiscMovement_t movement;
    MiscWorld_t    world;
    MiscExploits_t exploits;
    MiscInput_t    input;
};

// ──────────────── UI ────────────────
struct UIConfig_t {
    // background
    int   bg_style = 1;      // Particles
    float bg_intensity = 1.0f;   // 0.0 - 2.0
    float bg_speed = 1.0f;   // 0.0 - 3.0

    // window
    float window_alpha = 0.88f;  // 0.30 - 1.00

    // theme
    int   theme_preset = 0;      // 0=cold blue ... 5=rose, 6=custom
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