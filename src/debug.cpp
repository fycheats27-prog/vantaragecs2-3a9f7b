// language: C++, file: src/debug.cpp

#include "debug.h"
#include <Windows.h>
#include <cstdio>

namespace Debug {

    BhopState g_bhop;
    EspState  g_esp; // <-- AÑADIDO

    static DWORD s_last_flush = 0;

    void Flush() {
        DWORD now = GetTickCount();
        if (now - s_last_flush < 500) return;   // cada 500ms
        s_last_flush = now;

        char path[MAX_PATH];
        DWORD n = GetTempPathA(MAX_PATH, path);
        if (!n) return;
        strcat_s(path, "vanta_bhop.log");

        FILE* f = nullptr;
        fopen_s(&f, path, "w");   // truncar cada vez
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

        fprintf(f, "wrapper[00]   = %016llX\n", g_bhop.wrapper_00);
        fprintf(f, "wrapper[08]   = %016llX\n", g_bhop.wrapper_08);
        fprintf(f, "wrapper[10]   = %016llX\n", g_bhop.wrapper_10);
        fprintf(f, "wrapper[18]   = %016llX\n", g_bhop.wrapper_18);
        fprintf(f, "wrapper[20]   = %016llX\n", g_bhop.wrapper_20);
        fprintf(f, "wrapper[28]   = %016llX\n", g_bhop.wrapper_28);
        fprintf(f, "wrapper[30]   = %016llX\n", g_bhop.wrapper_30);
        fprintf(f, "wrapper[38]   = %016llX\n", g_bhop.wrapper_38);

        fclose(f);
    }

} // namespace Debug