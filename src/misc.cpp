#include "misc.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include <Windows.h>
#include <cstring>
#include <cstdio> // Para snprintf

namespace Features::Misc {

    namespace Offsets {
        constexpr uintptr_t dwEntityList = 0x271A908;
        constexpr uintptr_t dwGameRules = 0x271F8A0;
        constexpr uintptr_t dwLocalPlayerController = 0x253CB78;

        constexpr int m_flFlashDuration = 0x13F0;
        constexpr int m_flFlashMaxAlpha = 0x13F4;
        constexpr int m_bDidSmokeEffect = 0x11C8;
        constexpr int m_nSmokeEffectTickBegin = 0x1150;
        constexpr int m_szClan = 0x16C0;
        constexpr int m_bRankRevealAll = 0x9A0;
    }

    using ClientCmdFn = void(__fastcall*)(const char*, int);
    static ClientCmdFn g_client_cmd = nullptr;
    static uint32_t g_last_spam = 0;

    static void ApplyNoFlash(uintptr_t local) {
        if (local < 0x10000) return;
        __try {
            *(float*)(local + Offsets::m_flFlashDuration) = 0.0f;
            *(float*)(local + Offsets::m_flFlashMaxAlpha) = 0.0f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void ApplyNoSmoke(uintptr_t client) {
        uintptr_t es = *(uintptr_t*)(client + Offsets::dwEntityList);
        if (!es) return;
        int max_index = *(int*)(es + 0x2120);
        if (max_index <= 0 || max_index > 1024) max_index = 64;

        for (int i = 1; i <= max_index; ++i) {
            uintptr_t chunk = *(uintptr_t*)(es + 0x10 + 8 * ((i & 0x7FFF) >> 9));
            if (!chunk) continue;
            uintptr_t ent = *(uintptr_t*)(chunk + 0x70 * (i & 0x1FF));
            if (!ent) continue;

            uintptr_t id = *(uintptr_t*)(ent + 0x10);
            if (!id) continue;
            const char* designer = *(const char**)(id + 0x20);

            if (designer && strstr(designer, "smokegrenade_projectile") != nullptr) {
                __try {
                    *(bool*)(ent + Offsets::m_bDidSmokeEffect) = false;
                    *(int*)(ent + Offsets::m_nSmokeEffectTickBegin) = 0;
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }

    static void ApplyRevealRanks(uintptr_t client) {
        uintptr_t gr = *(uintptr_t*)(client + Offsets::dwGameRules);
        if (gr < 0x10000) return;
        __try {
            *(bool*)(gr + Offsets::m_bRankRevealAll) = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void ApplyClanTag(uintptr_t client) {
        if (!g_cfg.misc.exploits.clantag) return;
        uintptr_t controller = *(uintptr_t*)(client + Offsets::dwLocalPlayerController);
        if (controller < 0x10000) return;
        __try {
            char* p_tag = (char*)(controller + Offsets::m_szClan);
            const char* tag = g_cfg.misc.exploits.clantag_text;
            for (int i = 0; i < 15 && tag[i]; ++i) p_tag[i] = tag[i];
            p_tag[15] = '\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void ApplyChatSpam() {
        if (!g_cfg.misc.exploits.chat_spam || g_cfg.misc.exploits.chat_text[0] == '\0') return;
        uint32_t now = GetTickCount();
        if (now - g_last_spam < 1000) return;
        g_last_spam = now;

        if (!g_client_cmd) {
            HMODULE engine = GetModuleHandleA("engine2.dll");
            if (engine) g_client_cmd = (ClientCmdFn)GetProcAddress(engine, "ClientCmd");
        }
        if (g_client_cmd) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "say %s", g_cfg.misc.exploits.chat_text);
            g_client_cmd(cmd, 0);
        }
    }

    void Apply() {
        static uintptr_t client = 0;
        if (!client) client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        uintptr_t local = *(uintptr_t*)(client + 0x2565718);

        if (g_cfg.misc.world.no_flash)     ApplyNoFlash(local);
        if (g_cfg.misc.world.no_smoke)     ApplyNoSmoke(client);
        if (g_cfg.misc.world.reveal_ranks) ApplyRevealRanks(client);
        if (g_cfg.misc.exploits.clantag)   ApplyClanTag(client);
        if (g_cfg.misc.exploits.chat_spam) ApplyChatSpam();
    }
}