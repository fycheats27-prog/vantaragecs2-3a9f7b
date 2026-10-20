// --- debug.h ---
#pragma once
#include <cstdint>

namespace Debug {

    struct BhopState {
        int  hook_alive = 0;
        int  bhop_enabled = 0;
        int  tick = 0;
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

    // ---- CHAMS hook[0]: DrawArray @ scenesys+0x99C00 ----
    struct ChamsState {
        int       init_done = 0;
        int       hook_created = -1;
        int       hook_enabled = -1;
        uintptr_t scenesys = 0;
        uintptr_t hook_rva = 0;
        uint8_t   prologue[16] = { 0 };

        int       integrity_byte = -1;   // primer byte de la función tras 5s (0xE9 = hook vivo)

        int       fires = 0;
        uintptr_t a1 = 0;
        uintptr_t a2 = 0;
        uintptr_t a3 = 0;
        int       a4 = 0;
        int       a5 = 0;

        int       a1_probe_ok = 0;
        uintptr_t a1_p18 = 0;
        uintptr_t a1_p20 = 0;
        uintptr_t a1_p28 = 0;

        int       a3_probe_ok = 0;
        uintptr_t a3_p18 = 0;

        int       players_found = 0;
        uintptr_t last_scene = 0;
        uintptr_t last_owner = 0;
        int       last_hp = 0;

        int       material_ok = 0;
        uintptr_t material_ptr = 0;
        int       material_vt = -1;

        int       enabled = 1;
    };

    // ---- CHAMS hook[1]: GeneratePrimitives @ scenesys+0x27DEA0 ----
    struct ChamsGenPrimState {
        int       hook_created = -1;
        int       hook_enabled = -1;
        uintptr_t rva = 0;
        uint8_t   prologue[16] = { 0 };
        int       integrity_byte = -1;

        int       fires = 0;
        uintptr_t a1 = 0;
        uintptr_t a2 = 0;
        uintptr_t a3 = 0;
        int       a4 = 0;
        int       a5 = 0;

        int       a1_probe_ok = 0;
        uintptr_t a1_p18 = 0;
        uintptr_t a1_p20 = 0;
        uintptr_t a1_p28 = 0;

        int       a2_probe_ok = 0;
        uintptr_t a2_p18 = 0;
        uintptr_t a2_p20 = 0;
        uintptr_t a2_p28 = 0;

        int       a3_probe_ok = 0;
        uintptr_t a3_p18 = 0;
        uintptr_t a3_p20 = 0;
        uintptr_t a3_p28 = 0;

        int       players_found = 0;
        uintptr_t last_scene = 0;
        uintptr_t last_owner = 0;
        int       last_hp = 0;
    };

    // ---- CHAMS hook[2]: DrawModel scan en client.dll ----
    struct ChamsDrawModelState {
        int       matches = 0;
        int       created = -1;
        int       enabled = -1;
        uintptr_t rva = 0;
        uint8_t   prologue[16] = { 0 };
        int       integrity_byte = -1;

        int       fires = 0;
        uintptr_t a1 = 0;
        uintptr_t a2 = 0;
        uintptr_t a3 = 0;
        uintptr_t a1_owner = 0;
        int       a1_hp = 0;
        int       players_found = 0;
    };

    extern BhopState           g_bhop;
    extern ThirdPersonState    g_tp;
    extern EspState            g_esp;
    extern AimbotState         g_aim;
    extern ChamsState          g_chams;
    extern ChamsGenPrimState   g_chams_gp;
    extern ChamsDrawModelState g_chams_dm;

    void Flush();
}