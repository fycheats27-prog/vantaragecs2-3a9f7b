// --- chams.cpp ---
#include "chams.h"
#include "config.h"
#include "memory.h"
#include <MinHook.h>
#include <Windows.h>
#include <cstdint>
#include <vector>

namespace Features::Visuals::Chams {

    using GenPrimFn = __int64(__fastcall*)(__int64, __int64, __int64, __int64);
    static GenPrimFn o_GeneratePrimitives = nullptr;
    static void* g_mat = nullptr;
    static void* g_genprim = nullptr;
    static uintptr_t g_client = 0;

    struct MeshData {
        char pad0[0x18];
        void* scene;
        void* material;
        void* material2;
    };

    static uintptr_t PatternScan(uintptr_t base, size_t size, const char* pattern) {
        std::vector<int> bytes;
        const char* p = pattern;
        while (*p) {
            if (*p == ' ') { ++p; continue; }
            if (*p == '?') { bytes.push_back(-1); ++p; if (*p == '?') ++p; }
            else { bytes.push_back((int)strtoul(p, nullptr, 16)); p += 2; }
        }
        if (bytes.empty()) return 0;
        for (size_t i = 0; i + bytes.size() < size; ++i) {
            bool ok = true;
            for (size_t j = 0; j < bytes.size(); ++j)
                if (bytes[j] != -1 && *(uint8_t*)(base + i + j) != (uint8_t)bytes[j]) { ok = false; break; }
            if (ok) return base + i;
        }
        return 0;
    }

    static __int64 __fastcall hk_GeneratePrimitives(__int64 a1, __int64 a2, __int64 a3, __int64 a4) {
        if (!g_cfg.visuals.player.chams || !g_mat || !a3)
            return o_GeneratePrimitives ? o_GeneratePrimitives(a1, a2, a3, a4) : 0;

        __try {
            MeshData* mesh = (MeshData*)a3;
            if (!mesh->scene)
                return o_GeneratePrimitives(a1, a2, a3, a4);

            uintptr_t owner = *(uintptr_t*)((uintptr_t)mesh->scene + 0x30);
            if (owner < 0x10000 || owner > 0x7FFFFFFFFFFF)
                return o_GeneratePrimitives(a1, a2, a3, a4);

            int hp = *(int*)(owner + 0x34C);
            if (hp <= 0 || hp > 100)
                return o_GeneratePrimitives(a1, a2, a3, a4);

            void* orig = mesh->material;
            mesh->material = g_mat;
            mesh->material2 = g_mat;
            __int64 ret = o_GeneratePrimitives(a1, a2, a3, a4);
            mesh->material = orig;
            mesh->material2 = orig;
            return ret;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return o_GeneratePrimitives ? o_GeneratePrimitives(a1, a2, a3, a4) : 0;
        }
    }

    bool Init() {
        g_client = Memory::GetModuleBase("client.dll");
        uintptr_t scenesys = Memory::GetModuleBase("scenesystem.dll");
        if (!g_client || !scenesys) return false;

        // RVA confirmado
        g_genprim = (void*)(scenesys + 0x2F29D0);

        uintptr_t solid_ptr = *(uintptr_t*)(scenesys + 0x925BE0);
        if (solid_ptr > 0x10000)
            g_mat = (void*)solid_ptr;

        if (MH_CreateHook(g_genprim, &hk_GeneratePrimitives, (LPVOID*)&o_GeneratePrimitives) != MH_OK)
            return false;
        MH_EnableHook(g_genprim);
        return true;
    }

    void Shutdown() {
        if (g_genprim)
            MH_DisableHook(g_genprim);
    }
}