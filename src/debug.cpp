// --- debug.cpp ---
#include "debug.h"
#include <Windows.h>
#include <cstdio>

namespace Debug {

    BhopState g_bhop;
    ThirdPersonState g_tp;
    EspState  g_esp;
    AimbotState g_aim; // ✅ NUEVO

    static DWORD s_last_flush = 0;

    void Flush() {
        DWORD now = GetTickCount();
        if (now - s_last_flush < 500) return;
        s_last_flush = now;

        char path[MAX_PATH];
        DWORD n = GetTempPathA(MAX_PATH, path);
        if (!n) return;
        strcat_s(path, "vanta_bhop.log");

        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        if (!f) return;

        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f, "VANTA DIAG  %02d:%02d:%02d.%03d\n\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        fprintf(f, "hook_alive    = %d\n", g_bhop.hook_alive);
        fprintf(f, "bhop_enabled  = %d\n", g_bhop.bhop_enabled);
        fprintf(f, "tick          = %d\n\n", g_bhop.tick);

        fprintf(f, "cmd           = %p\n", (void*)g_bhop.cmd);
        fprintf(f, "wrapper       = %p\n", (void*)g_bhop.wrapper);
        fprintf(f, "pawn          = %p\n\n", (void*)g_bhop.pawn);

        fprintf(f, "hp            = %d\n", g_bhop.hp);
        fprintf(f, "team          = %d\n", g_bhop.team);
        fprintf(f, "life          = %d\n", g_bhop.life);
        fprintf(f, "flags         = 0x%08X\n", g_bhop.flags);
        fprintf(f, "on_ground     = %d\n", g_bhop.on_ground ? 1 : 0);
        fprintf(f, "jump_held     = %d\n\n", g_bhop.jump_held ? 1 : 0);

        fprintf(f, "ESP FOUND     = %d\n", g_esp.entities_found);
        fprintf(f, "ESP ONSCREEN  = %d\n", g_esp.entities_on_screen);
        fprintf(f, "LOCAL ORIGIN  = %.1f, %.1f, %.1f\n", g_esp.local_x, g_esp.local_y, g_esp.local_z);
        fprintf(f, "TARGET ORIGIN = %.1f, %.1f, %.1f\n\n", g_esp.target_x, g_esp.target_y, g_esp.target_z);

        // ✅ NUEVO: Bloque del Aimbot
        fprintf(f, "\nAIMBOT\n");
        fprintf(f, "active        = %d\n", g_aim.active);
        fprintf(f, "target_idx    = %d\n", g_aim.target_idx);
        fprintf(f, "target_dist   = %.1f\n", g_aim.target_dist);
        fprintf(f, "aim_angles    = %.1f, %.1f\n", g_aim.aim_pitch, g_aim.aim_yaw);
        fprintf(f, "cur_angles    = %.1f, %.1f\n", g_aim.cur_pitch, g_aim.cur_yaw);
        fprintf(f, "fov_limit     = %d\n", g_aim.fov);
        fprintf(f, "bone_idx      = %d\n\n", g_aim.bone);

        fprintf(f, "wrapper[00]   = %016llX\n", g_bhop.wrapper_00);
        fprintf(f, "wrapper[08]   = %016llX\n", g_bhop.wrapper_08);
        fprintf(f, "wrapper[10]   = %016llX\n", g_bhop.wrapper_10);
        fprintf(f, "wrapper[18]   = %016llX\n", g_bhop.wrapper_18);
        fprintf(f, "wrapper[20]   = %016llX\n", g_bhop.wrapper_20);
        fprintf(f, "wrapper[28]   = %016llX\n", g_bhop.wrapper_28);
        fprintf(f, "wrapper[30]   = %016llX\n", g_bhop.wrapper_30);
        fprintf(f, "wrapper[38]   = %016llX\n", g_bhop.wrapper_38);

        fprintf(f, "\nTHIRD PERSON\n");
        fprintf(f, "menu_on       = %d\n", g_tp.menu_on);
        fprintf(f, "input         = %p\n", (void*)g_tp.input);
        fprintf(f, "ticks         = %d\n", g_tp.ticks);
        fprintf(f, "flag 0x250    = %d\n", g_tp.v250);
        fprintf(f, "flag 0x228    = %d\n", g_tp.v228);
        fprintf(f, "flag 0x229    = %d\n", g_tp.v229);
        fprintf(f, "flag 0x251    = %d\n", g_tp.v251);

        fclose(f);
    }

} // namespace Debug