// --- debug.cpp ---
#include "debug.h"
#include <Windows.h>
#include <cstdio>

namespace Debug {

    MovementState    g_movement;
    ThirdPersonState g_tp;
    EspState         g_esp;
    AimbotState      g_aim;
    ChamsState       g_chams;
    MiscState        g_misc;

    static DWORD s_last_flush = 0;

    void Flush() {
        DWORD now = GetTickCount();
        if (now - s_last_flush < 500) return;
        s_last_flush = now;

        char path[MAX_PATH];
        if (!GetTempPathA(MAX_PATH, path)) return;
        strcat_s(path, "vanta.log");

        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        if (!f) return;

        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f, "VANTA DIAG  %02d:%02d:%02d.%03d\n\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        // ---- MOVEMENT / CreateMove ----
        fprintf(f, "MOVEMENT\n");
        fprintf(f, "hook_alive    = %d\n", g_movement.hook_alive);
        fprintf(f, "hook_addr     = 0x%llX\n", (unsigned long long)g_movement.hook_addr);
        fprintf(f, "calls         = %d\n", g_movement.calls);
        fprintf(f, "on_ground     = %d\n", g_movement.on_ground);
        fprintf(f, "jump_held     = %d\n", g_movement.jump_held);
        fprintf(f, "bhop_active   = %d\n", g_movement.bhop_active);
        fprintf(f, "fast_active   = %d\n", g_movement.fast_active);
        fprintf(f, "counter_active= %d\n\n", g_movement.counter_active);

        // ---- ESP ----
        fprintf(f, "ESP\n");
        fprintf(f, "FOUND         = %d\n", g_esp.entities_found);
        fprintf(f, "ONSCREEN      = %d\n", g_esp.entities_on_screen);
        fprintf(f, "LOCAL ORIGIN  = %.1f, %.1f, %.1f\n", g_esp.local_x, g_esp.local_y, g_esp.local_z);
        fprintf(f, "TARGET ORIGIN = %.1f, %.1f, %.1f\n\n", g_esp.target_x, g_esp.target_y, g_esp.target_z);

        // ---- AIMBOT ----
        fprintf(f, "AIMBOT\n");
        fprintf(f, "active        = %d\n", g_aim.active);
        fprintf(f, "target_idx    = %d\n", g_aim.target_idx);
        fprintf(f, "target_dist   = %.1f\n", g_aim.target_dist);
        fprintf(f, "aim_angles    = %.1f, %.1f\n", g_aim.aim_pitch, g_aim.aim_yaw);
        fprintf(f, "cur_angles    = %.1f, %.1f\n", g_aim.cur_pitch, g_aim.cur_yaw);
        fprintf(f, "fov_limit     = %d\n", g_aim.fov);
        fprintf(f, "bone_idx      = %d\n\n", g_aim.bone);

        // ---- CHAMS ----
        fprintf(f, "CHAMS\n");
        fprintf(f, "enabled       = %d\n", g_chams.enabled);
        fprintf(f, "init_done     = %d\n", g_chams.init_done);
        fprintf(f, "draws/frame   = %d\n", g_chams.draws);
        fprintf(f, "painted/frame = %d\n", g_chams.painted);
        fprintf(f, "stride        = 0x%X\n", g_chams.stride);
        fprintf(f, "mode          = %d\n\n", g_chams.mode);

        // ---- MISC ----
        fprintf(f, "MISC\n");
        fprintf(f, "no_flash      = %d\n", g_misc.no_flash);
        fprintf(f, "no_smoke      = %d\n", g_misc.no_smoke);
        fprintf(f, "reveal_ranks  = %d\n", g_misc.reveal_ranks);
        fprintf(f, "clantag       = %d\n", g_misc.clantag);
        fprintf(f, "chat_spam     = %d\n\n", g_misc.chat_spam);

        // ---- THIRD PERSON ----
        fprintf(f, "THIRD PERSON\n");
        fprintf(f, "menu_on       = %d\n", g_tp.menu_on);
        fprintf(f, "input         = 0x%llX\n", (unsigned long long)g_tp.input);
        fprintf(f, "ticks         = %d\n", g_tp.ticks);
        fprintf(f, "flag 0x250    = %d\n", g_tp.v250);
        fprintf(f, "flag 0x228    = %d\n", g_tp.v228);
        fprintf(f, "flag 0x229    = %d\n", g_tp.v229);
        fprintf(f, "flag 0x251    = %d\n", g_tp.v251);

        fclose(f);
    }

} // namespace Debug