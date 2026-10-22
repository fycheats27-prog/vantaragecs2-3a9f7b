// --- debug.h ---
#pragma once
#include <cstdint>

namespace Debug {

    struct MovementState {
        int       hook_alive = 0;   // 1 si CreateMove fue hookeado
        uintptr_t hook_addr = 0;   // dirección real de CreateMove
        int       calls = 0;   // veces que disparó el hook

        int       on_ground = 0;   // FL_ONGROUND
        int       jump_held = 0;   // espacio presionado
        int       bhop_active = 0;   // forzó IN_JUMP este tick
        int       fast_active = 0;   // zeroeó velocity
        int       counter_active = 0; // invirtió vector
        int       access_violation_count = 0;
    };

    struct ThirdPersonState {
        int  menu_on = 0;
        uintptr_t input = 0;
        int  ticks = 0;
        int  v250 = 0, v228 = 0, v229 = 0, v251 = 0;
    };

    struct EspState {
        int entities_found = 0;
        int entities_on_screen = 0;
        float local_x = 0.f, local_y = 0.f, local_z = 0.f;
        float target_x = 0.f, target_y = 0.f, target_z = 0.f;
    };

    struct AimbotState {
        int  active = 0;
        int  target_idx = -1;
        float target_dist = 0.f;
        float aim_pitch = 0.f;
        float aim_yaw = 0.f;
        float cur_pitch = 0.f;
        float cur_yaw = 0.f;
        int  fov = 0;
        int  bone = 0;
    };

    struct ChamsState {
        int       enabled = 1;
        int       init_done = 0;
        int       draws = 0;   // draws procesados este frame
        int       painted = 0;   // draws pintados este frame
        int       stride = 0;   // stride activo
        int       mode = 0;   // 0=mag, 1=red, 2=blue
    };

    struct MiscState {
        int no_flash = 0;
        int no_smoke = 0;
        int reveal_ranks = 0;
        int clantag = 0;
        int chat_spam = 0;
    };

    extern MovementState    g_movement;
    extern ThirdPersonState g_tp;
    extern EspState         g_esp;
    extern AimbotState      g_aim;
    extern ChamsState       g_chams;
    extern MiscState        g_misc;

    void Flush();
}