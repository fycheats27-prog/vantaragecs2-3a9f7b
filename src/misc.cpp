// language: C++, file: src/misc.cpp
#include "misc.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include <Windows.h>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <algorithm> // Requerido para std::clamp
#include <random>    // Requerido para el delay aleatorio

namespace Features::Misc {

    // ── Offsets (sincronizados con dump) ─────────────────────────────────────
    namespace Offsets {
        constexpr uintptr_t dwEntityList = 0x271A908;
        constexpr uintptr_t dwLocalPlayerPawn = 0x2565718;
        constexpr uintptr_t dwLocalPlayerController = 0x253CB78;
        constexpr uintptr_t dwGameRules = 0x25651D0;

        constexpr int m_flFlashMaxAlpha = 0x150C;
        constexpr int m_flFlashDuration = 0x1510;
        constexpr int m_bDidSmokeEffect = 0x1364;
        constexpr int m_nSmokeEffectTickBegin = 0x1360;
        constexpr int m_szClan = 0x868;
    }

    // ── Estados Globales de Características ──────────────────────────────────
    static const char* clean_chars[] = {
        "\xE2\x80\x8B", // \u200B (Zero-width space)
        "\xE2\x80\x8C", // \u200C (Zero-width non-joiner)
        "\xE2\x80\x8D", // \u200D (Zero-width joiner)
        "\xEF\xBB\xBF"  // \uFEFF (Zero-width no-break space / BOM)
    };
    static int clean_index = 0;
    static auto last_clean_update = std::chrono::steady_clock::now();

    // Variables para el Chat Spam Aleatorio
    static auto last_spam_time = std::chrono::steady_clock::now();
    static int next_random_delay = 1500; // Delay inicial por defecto (en milisegundos)

    // ── Métodos Utilitarios Internos ─────────────────────────────────────────
    static void* GetEngineClient() {
        static void* p_engine_client = nullptr;
        if (!p_engine_client) {
            HMODULE engine_module = GetModuleHandleA("engine2.dll");
            if (engine_module) {
                using CreateInterfaceFn = void* (*)(const char*, int*);
                auto create_interface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(engine_module, "CreateInterface"));
                if (create_interface) {
                    p_engine_client = create_interface("Source2EngineToClient001", nullptr);
                }
            }
        }
        return p_engine_client;
    }

    static void SafeExecCmd(const char* cmd) {
        void* engine = GetEngineClient();
        if (!engine) return;

        using ExecuteClientCmdFn = void(__thiscall*)(void*, int, const char*, int);

        void** vtable = *reinterpret_cast<void***>(engine);
        auto oExecuteClientCmd = reinterpret_cast<ExecuteClientCmdFn>(vtable[50]);

        if (oExecuteClientCmd) {
            __try {
                oExecuteClientCmd(engine, 0, cmd, 0x7FFEF001);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    static uintptr_t GetEntity(uintptr_t entity_list, int index) {
        if (index <= 0 || index > 1024) return 0;
        uintptr_t entry_index = index & 0x7FFF;
        uintptr_t list_entry_ptr = *reinterpret_cast<uintptr_t*>(entity_list + 0x10 + 8 * (entry_index >> 9));
        if (!list_entry_ptr) return 0;
        return *reinterpret_cast<uintptr_t*>(list_entry_ptr + 0x70 * (entry_index & 0x1FF));
    }

    // ── Lógica de Características de Exploits ───────────────────────────────

    void AntiUntrusted(float& pitch, float& yaw, float& roll) {
        if (!g_cfg.misc.exploits.anti_untrusted) return;

        while (yaw > 180.0f)  yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;

        pitch = std::clamp(pitch, -89.0f, 89.0f);
        roll = 0.0f;
    }

    static void ApplyNoFlash(uintptr_t local) {
        if (local < 0x10000) return;
        __try {
            *reinterpret_cast<float*>(local + Offsets::m_flFlashDuration) = 0.0f;
            *reinterpret_cast<float*>(local + Offsets::m_flFlashMaxAlpha) = 0.0f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void ApplyNoSmoke(uintptr_t client) {
        uintptr_t es_base = *reinterpret_cast<uintptr_t*>(client + Offsets::dwEntityList);
        if (!es_base) return;
        for (int i = 1; i <= 512; ++i) {
            uintptr_t ent = GetEntity(es_base, i);
            if (!ent) continue;
            __try {
                bool* did_smoke = reinterpret_cast<bool*>(ent + Offsets::m_bDidSmokeEffect);
                int* tick_begin = reinterpret_cast<int*>(ent + Offsets::m_nSmokeEffectTickBegin);
                if (*did_smoke) {
                    *did_smoke = false;
                    *tick_begin = 0;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        }
    }

    static void ApplyCleanTag(uintptr_t client) {
        if (!g_cfg.misc.exploits.clantag) return;

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_clean_update).count() > 500) {
            clean_index = (clean_index + 1) % 4;
            last_clean_update = now;

            char cmd[64];
            std::snprintf(cmd, sizeof(cmd), "cl_clanid \"%s\"", clean_chars[clean_index]);
            SafeExecCmd(cmd);
        }
    }

    static void ApplyChatSpam() {
        if (!g_cfg.misc.exploits.chat_spam || g_cfg.misc.exploits.chat_text[0] == '\0') return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_spam_time).count();

        // Si no ha pasado el tiempo aleatorio calculado previamente, no enviamos nada
        if (elapsed < next_random_delay) return;

        // Actualizamos el último tiempo de envío
        last_spam_time = now;

        // Enviamos el comando de chat a través de la VTable corregida
        char cmd[512];
        std::snprintf(cmd, sizeof(cmd), "say %s", g_cfg.misc.exploits.chat_text);
        SafeExecCmd(cmd);

        // Generamos un nuevo delay aleatorio para el PRÓXIMO mensaje (ej. entre 1100 y 2300 ms)
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1100, 2300); // Rango seguro anti-spam filter
        next_random_delay = dist(gen);
    }

    void Apply() {
        static uintptr_t client = 0;
        if (!client) client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        uintptr_t local_pawn = *reinterpret_cast<uintptr_t*>(client + Offsets::dwLocalPlayerPawn);

        if (g_cfg.misc.world.no_flash)     ApplyNoFlash(local_pawn);
        if (g_cfg.misc.world.no_smoke)     ApplyNoSmoke(client);

        ApplyCleanTag(client);
        ApplyChatSpam();
    }

} // namespace Features::Misc
