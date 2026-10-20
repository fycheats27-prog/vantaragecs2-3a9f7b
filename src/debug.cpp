// --- debug.cpp ---
#include "debug.h"
#include <Windows.h>
#include <cstdio>

namespace Debug {

    BhopState           g_bhop;
    ThirdPersonState    g_tp;
    EspState            g_esp;
    AimbotState         g_aim;
    ChamsState          g_chams;
    ChamsGenPrimState   g_chams_gp;
    ChamsDrawModelState g_chams_dm;

    static DWORD s_last_flush = 0;

    static inline uint8_t SafeReadByte(uintptr_t addr, int* ok) {
        *ok = 0;
        if (!addr) return 0;
        __try {
            uint8_t v = *(uint8_t*)addr;
            *ok = 1;
            return v;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static void VerifyHookBytes() {
        int ok99 = 0, okgp = 0, okdm = 0;

        if (g_chams.scenesys && g_chams.scenesys != 0xDEADBEEF) {
            g_chams.integrity_byte = SafeReadByte(g_chams.scenesys + 0x99C00, &ok99);
            g_chams_gp.integrity_byte = SafeReadByte(g_chams.scenesys + 0x27DEA0, &okgp);
        }
        if (g_chams_dm.rva && g_chams_dm.rva != 0 && g_chams_dm.prologue[0] != 0) {
            // no tenemos la base de client aquí sin llamar GetModuleBase; usamos el rva guardado
            // y lo pedimos prestado una vez estática
            static uintptr_t s_client = 0;
            if (!s_client) {
                HMODULE h = GetModuleHandleA("client.dll");
                if (h) s_client = (uintptr_t)h;
            }
            if (s_client) {
                g_chams_dm.integrity_byte = SafeReadByte(s_client + g_chams_dm.rva, &okdm);
            }
        }
    }

    void Flush() {
        DWORD now = GetTickCount();
        if (now - s_last_flush < 500) return;
        s_last_flush = now;

        // verificación de integridad en cada flush, sin thread aparte
        VerifyHookBytes();

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

        fprintf(f, "cmd           = 0x%llX\n", (unsigned long long)g_bhop.cmd);
        fprintf(f, "wrapper       = 0x%llX\n", (unsigned long long)g_bhop.wrapper);
        fprintf(f, "pawn          = 0x%llX\n\n", (unsigned long long)g_bhop.pawn);

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

        fprintf(f, "\nCHAMS\n");
        fprintf(f, "enabled       = %d\n", g_chams.enabled);
        fprintf(f, "init_done     = %d\n", g_chams.init_done);
        fprintf(f, "hook_created  = %d\n", g_chams.hook_created);
        fprintf(f, "hook_enabled  = %d\n", g_chams.hook_enabled);
        fprintf(f, "scenesys      = 0x%llX\n", (unsigned long long)g_chams.scenesys);
        fprintf(f, "hook_rva      = 0x%llX\n", (unsigned long long)g_chams.hook_rva);
        fprintf(f, "prologue      =");
        for (int i = 0; i < 16; ++i) fprintf(f, " %02X", g_chams.prologue[i]);
        fprintf(f, "\n");
        fprintf(f, "fires         = %d\n", g_chams.fires);
        fprintf(f, "last a1       = 0x%llX\n", (unsigned long long)g_chams.a1);
        fprintf(f, "last a2       = 0x%llX\n", (unsigned long long)g_chams.a2);
        fprintf(f, "last a3       = 0x%llX\n", (unsigned long long)g_chams.a3);
        fprintf(f, "last a4       = %d\n", g_chams.a4);
        fprintf(f, "last a5       = 0x%X\n", g_chams.a5);
        fprintf(f, "a1 probe ok   = %d\n", g_chams.a1_probe_ok);
        fprintf(f, "a1+0x18       = 0x%llX\n", (unsigned long long)g_chams.a1_p18);
        fprintf(f, "a1+0x20       = 0x%llX\n", (unsigned long long)g_chams.a1_p20);
        fprintf(f, "a1+0x28       = 0x%llX\n", (unsigned long long)g_chams.a1_p28);
        fprintf(f, "players found = %d\n", g_chams.players_found);
        fprintf(f, "material ok   = %d\n", g_chams.material_ok);
        fprintf(f, "material ptr  = 0x%llX\n", (unsigned long long)g_chams.material_ptr);

        fprintf(f, "\nCHAMS GP (scenesys+0x%llX)\n", (unsigned long long)g_chams_gp.rva);
        fprintf(f, "gp_hook_created = %d\n", g_chams_gp.hook_created);
        fprintf(f, "gp_hook_enabled = %d\n", g_chams_gp.hook_enabled);
        fprintf(f, "gp_prologue     =");
        for (int i = 0; i < 16; ++i) fprintf(f, " %02X", g_chams_gp.prologue[i]);
        fprintf(f, "\n");
        fprintf(f, "gp_fires        = %d\n", g_chams_gp.fires);
        fprintf(f, "gp_a1           = 0x%llX\n", (unsigned long long)g_chams_gp.a1);
        fprintf(f, "gp_a2           = 0x%llX\n", (unsigned long long)g_chams_gp.a2);
        fprintf(f, "gp_a3           = 0x%llX\n", (unsigned long long)g_chams_gp.a3);
        fprintf(f, "gp_players      = %d\n", g_chams_gp.players_found);

        fprintf(f, "\nHOOK INTEGRITY\n");
        fprintf(f, "int_byte_99    = 0x%02X (%s)\n",
            g_chams.integrity_byte & 0xFF,
            g_chams.integrity_byte == 0xE9 ? "ALIVE"
            : (g_chams.integrity_byte <= 0 ? "no read" : "REVERTED"));
        fprintf(f, "int_byte_gp    = 0x%02X (%s)\n",
            g_chams_gp.integrity_byte & 0xFF,
            g_chams_gp.integrity_byte == 0xE9 ? "ALIVE"
            : (g_chams_gp.integrity_byte <= 0 ? "no read" : "REVERTED"));

        fprintf(f, "\nCHAMS DM (client.dll)\n");
        fprintf(f, "dm_matches     = %d\n", g_chams_dm.matches);
        fprintf(f, "dm_created     = %d\n", g_chams_dm.created);
        fprintf(f, "dm_enabled     = %d\n", g_chams_dm.enabled);
        fprintf(f, "dm_rva         = 0x%llX\n", (unsigned long long)g_chams_dm.rva);
        fprintf(f, "dm_prologue    =");
        for (int i = 0; i < 16; ++i) fprintf(f, " %02X", g_chams_dm.prologue[i]);
        fprintf(f, "\n");
        fprintf(f, "dm_fires       = %d\n", g_chams_dm.fires);
        fprintf(f, "dm_a1          = 0x%llX\n", (unsigned long long)g_chams_dm.a1);
        fprintf(f, "dm_a1_hp       = %d\n", g_chams_dm.a1_hp);
        fprintf(f, "dm_players     = %d\n", g_chams_dm.players_found);

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