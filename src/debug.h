// --- debug.h ---
#pragma once
#include <cstdint>

namespace Debug {

    struct BhopState {
        int  hook_alive = 0;
        int  bhop_enabled = 0;
        int  tick = 0;
        // Cambiamos void* por uintptr_t para evitar errores de conversión en bhop.cpp
        uintptr_t cmd = 0;
        uintptr_t wrapper = 0;
        uintptr_t pawn = 0;
        int  hp = 0, team = 0, life = 0;
        uint32_t flags = 0;
        bool on_ground = false, jump_held = false;
        uint64_t wrapper_00 = 0, wrapper_08 = 0, wrapper_10 = 0, wrapper_18 = 0;
        uint64_t wrapper_20 = 0, wrapper_28 = 0, wrapper_30 = 0, wrapper_38 = 0;
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

    extern BhopState g_bhop;
    extern ThirdPersonState g_tp;
    extern EspState g_esp;
    extern AimbotState g_aim;

    void Flush();
}