// language: C++, file: src/debug.h
#pragma once
#include <cstdint>

namespace Debug {

    struct BhopState {
        bool  hook_alive = false;
        bool  bhop_enabled = false;
        int   tick = 0;
        uintptr_t cmd = 0;
        uintptr_t wrapper = 0;
        uintptr_t pawn = 0;
        int   hp = -1;
        int   team = -1;
        int   life = -1;
        uint32_t flags = 0;
        bool  on_ground = false;
        bool  jump_held = false;
        uint64_t wrapper_00 = 0;
        uint64_t wrapper_08 = 0;
        uint64_t wrapper_10 = 0;
        uint64_t wrapper_18 = 0;
        uint64_t wrapper_20 = 0;
        uint64_t wrapper_28 = 0;
        uint64_t wrapper_30 = 0;
        uint64_t wrapper_38 = 0;
    };

    extern BhopState g_bhop;

    // ── Estado del ESP ──
    struct EspState {
        int entities_found = 0;
        int entities_on_screen = 0;
        float local_x = 0.0f, local_y = 0.0f, local_z = 0.0f;
        float target_x = 0.0f, target_y = 0.0f, target_z = 0.0f;
    };
    extern EspState g_esp;

    void Flush();
}