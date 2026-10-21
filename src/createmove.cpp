#include "createmove.h"
#include "bhop.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include <Windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cstdlib>

namespace Hooks::CreateMove {

    // Pattern de CreateMove en client.dll
    static const char* kCreateMovePattern = "85 D2 0F 85 ? ? ? ? 48 8B C4 44 88 40 18";

    // Firma real: (CCSGOInput*, int slot, CUserCmd*, float flInputSampleTime)
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
        // Llamar al original primero con TODOS los argumentos
        bool result = o_CreateMove ? o_CreateMove(pInput, slot, pCmd, flInputSampleTime) : false;

        if (pCmd) {
            Features::Misc::Bhop::OnCreateMove(pCmd);
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