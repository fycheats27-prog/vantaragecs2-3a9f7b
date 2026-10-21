// --- debug.cpp ---
#include "debug.h"
#include <Windows.h>
#include <cstdio>

namespace Debug {

    BhopState        g_bhop;
    ThirdPersonState g_tp;
    EspState         g_esp;
    AimbotState      g_aim;
    ChamsState       g_chams;

    static DWORD s_last_flush = 0;

    void Flush() {
        DWORD now = GetTickCount();
        if (now - s_last_flush < 500) return;
        s_last_flush = now;

        char path[MAX_PATH];
        if (!GetTempPathA(MAX_PATH, path)) return;
        strcat_s(path, "vanta_bhop.log");

        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        if (!f) return;

        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f, "VANTA DIAG  %02d:%02d:%02d.%03d\n\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        // ---- BHOP ----
        fprintf(f, "hook_alive    = %d\n", g_bhop.hook_alive);
        fprintf(f, "bhop_enabled  = %d\n", g_bhop.bhop_enabled);
        fprintf(f, "tick          = %d\n\n", g_bhop.tick);

        fprintf(f, "cmd           = 0x%llX\n", (unsigned long long)g_bhop.cmd);
        fprintf(f, "wrapper       = 0x%llX\n", (unsigned long long)g_bhop.wrapper);
        fprintf(f, "pawn          = 0x%llX\n\n", (unsigned long long)g_bhop.pawn);

        fprintf(f, "hp            = %d\n", g_bhop.hp);
        fprintf(f, "team          = %d\n", g_bhop.team);
        fprintf(f, "life          = %d\n", g_bhop.life);
        fprintf(f, "flags         = 0x%08X\n", g_bhop.flags);
        fprintf(f, "on_ground     = %d\n", g_bhop.on_ground ? 1 : 0);
        fprintf(f, "jump_held     = %d\n\n", g_bhop.jump_held ? 1 : 0);

        // ---- ESP ----
        fprintf(f, "ESP FOUND     = %d\n", g_esp.entities_found);
        fprintf(f, "ESP ONSCREEN  = %d\n", g_esp.entities_on_screen);
        fprintf(f, "LOCAL ORIGIN  = %.1f, %.1f, %.1f\n", g_esp.local_x, g_esp.local_y, g_esp.local_z);
        fprintf(f, "TARGET ORIGIN = %.1f, %.1f, %.1f\n\n", g_esp.target_x, g_esp.target_y, g_esp.target_z);

        // ---- AIMBOT ----
        fprintf(f, "\nAIMBOT\n");
        fprintf(f, "active        = %d\n", g_aim.active);
        fprintf(f, "target_idx    = %d\n", g_aim.target_idx);
        fprintf(f, "target_dist   = %.1f\n", g_aim.target_dist);
        fprintf(f, "aim_angles    = %.1f, %.1f\n", g_aim.aim_pitch, g_aim.aim_yaw);
        fprintf(f, "cur_angles    = %.1f, %.1f\n", g_aim.cur_pitch, g_aim.cur_yaw);
        fprintf(f, "fov_limit     = %d\n", g_aim.fov);
        fprintf(f, "bone_idx      = %d\n\n", g_aim.bone);

        // ---- wrapper dump ----
        fprintf(f, "wrapper[00]   = %016llX\n", g_bhop.wrapper_00);
        fprintf(f, "wrapper[08]   = %016llX\n", g_bhop.wrapper_08);
        fprintf(f, "wrapper[10]   = %016llX\n", g_bhop.wrapper_10);
        fprintf(f, "wrapper[18]   = %016llX\n", g_bhop.wrapper_18);
        fprintf(f, "wrapper[20]   = %016llX\n", g_bhop.wrapper_20);
        fprintf(f, "wrapper[28]   = %016llX\n", g_bhop.wrapper_28);
        fprintf(f, "wrapper[30]   = %016llX\n", g_bhop.wrapper_30);
        fprintf(f, "wrapper[38]   = %016llX\n", g_bhop.wrapper_38);

        // ---- CHAMS ----
        fprintf(f, "\nCHAMS\n");
        fprintf(f, "enabled       = %d\n", g_chams.enabled);
        fprintf(f, "init_done     = %d\n", g_chams.init_done);
        fprintf(f, "scenesys      = 0x%llX\n", (unsigned long long)g_chams.scenesys);
        fprintf(f, "total_hooks   = %d\n", g_chams.total_hooks);
        fprintf(f, "good_idx      = %d\n", g_chams.good_idx);
        fprintf(f, "good_slot     = 0x%llX\n", (unsigned long long)g_chams.good_rva);
        fprintf(f, "good_fires    = %d\n", g_chams.good_fires);
        fprintf(f, "good_players  = %d\n", g_chams.good_players);
        fprintf(f, "stride_used   = 0x%X\n", g_chams.stride_used);
        fprintf(f, "read_only     = %d\n", g_chams.read_only);
        fprintf(f, "write_count   = %d\n", g_chams.write_count);
        fprintf(f, "material_ok   = %d\n", g_chams.material_ok);
        fprintf(f, "material_ptr  = 0x%llX\n", (unsigned long long)g_chams.material_ptr);

        fprintf(f, "hook fires    =");
        {
            int total = g_chams.total_hooks > 40 ? 40 : g_chams.total_hooks;
            for (int i = 0; i < total; ++i)
                fprintf(f, " %d", g_chams.hook_fires[i]);
        }
        fprintf(f, "\n");

        // ---- THIRD PERSON ----
        fprintf(f, "\nTHIRD PERSON\n");
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