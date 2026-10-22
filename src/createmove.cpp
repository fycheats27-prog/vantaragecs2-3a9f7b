// language: C++, file: src/createmove.cpp
#include "createmove.h"
#include "bhop.h"
#include "misc.h"  
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include <Windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cstdlib>

namespace Hooks::CreateMove {

    static const char* kCreateMovePattern = "85 D2 0F 85 ? ? ? ? 48 8B C4 44 88 40 18";

    using CreateMoveFn = bool(__fastcall*)(void* pInput, int slot, void* pCmd, float flInputSampleTime);

    static CreateMoveFn o_CreateMove = nullptr;
    static void* g_create_move_addr = nullptr;

    static uintptr_t PatternScan(uintptr_t base, size_t size, const char* pattern) {
        size_t pat_len = 0;
        for (const char* p = pattern; *p; ) {
            if (*p == ' ') { ++p; continue; }
            if (*p == '?') { ++p; if (*p == '?') ++p; }
            else p += 2;
            ++pat_len;
        }

        uint8_t* bytes = (uint8_t*)std::malloc(pat_len);
        bool* mask = (bool*)std::malloc(pat_len);
        if (!bytes || !mask) { std::free(bytes); std::free(mask); return 0; }

        size_t idx = 0;
        for (const char* p = pattern; *p; ) {
            if (*p == ' ') { ++p; continue; }
            if (*p == '?') { mask[idx] = false; bytes[idx] = 0; ++p; if (*p == '?') ++p; }
            else { mask[idx] = true; bytes[idx] = (uint8_t)std::strtoul(p, nullptr, 16); p += 2; }
            ++idx;
        }

        uintptr_t found = 0;
        for (size_t i = 0; i + pat_len <= size; ++i) {
            bool ok = true;
            for (size_t j = 0; j < pat_len; ++j) {
                if (mask[j] && ((uint8_t*)base)[i + j] != bytes[j]) { ok = false; break; }
            }
            if (ok) { found = base + i; break; }
        }

        std::free(bytes);
        std::free(mask);
        return found;
    }

    static bool __fastcall hk_CreateMove(void* pInput, int slot, void* pCmd, float flInputSampleTime) {
        // Ejecutamos el método original del juego primero
        bool result = o_CreateMove ? o_CreateMove(pInput, slot, pCmd, flInputSampleTime) : false;

        if (pCmd) {
            // Ejecutar funciones básicas
            Features::Misc::Bhop::OnCreateMove(pCmd);

            // COMPROBACIÓN EXTREMA ANTI-CRASH:
            // Verificamos de forma segura mediante SEH (Structured Exception Handling) que los punteros sean válidos
            __try {
                // En CS2, pCmd apunta a la estructura CUserCmd. 
                // Usamos un casteo defensivo para verificar si la dirección base es una zona de memoria accesible.
                uintptr_t pUserCmd = reinterpret_cast<uintptr_t>(pCmd);

                // Leemos el puntero intermedio con un offset base seguro de validación
                if (pUserCmd > 0x10000) {
                    uintptr_t pBaseCmd = *reinterpret_cast<uintptr_t*>(pUserCmd + 0x18);

                    if (pBaseCmd > 0x10000) {
                        // Offset alternativo nativo y más estable en las estructuras internas sincronizadas de CS2
                        float* view_angles = reinterpret_cast<float*>(pBaseCmd + 0x20);

                        if (view_angles && !IsBadWritePtr(view_angles, sizeof(float) * 3)) {
                            float& pitch = view_angles[0];
                            float& yaw = view_angles[1];
                            float& roll = view_angles[2];

                            // Aplicamos la normalización y el límite anti-untrusted de forma segura
                            Features::Misc::AntiUntrusted(pitch, yaw, roll);
                        }
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                // Si la build del juego cambió la estructura interna de CUserCmd, el código atrapa el crash 
                // silenciosamente aquí sin colgar tu Counter-Strike 2.
            }
        }

        return result;
    }

    bool Init() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return false;

        g_create_move_addr = (void*)PatternScan(client, 0x4000000, kCreateMovePattern);
        if (!g_create_move_addr) return false;

        MH_STATUS st = MH_CreateHook(g_create_move_addr, &hk_CreateMove,
            reinterpret_cast<LPVOID*>(&o_CreateMove));
        if (st != MH_OK) return false;

        MH_EnableHook(g_create_move_addr);
        return true;
    }

    void Shutdown() {
        if (g_create_move_addr) {
            MH_DisableHook(g_create_move_addr);
            g_create_move_addr = nullptr;
        }
    }
}
