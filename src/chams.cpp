// language: C++, file: src/chams.cpp
// *Velocity-style: hook GeneratePrimitives, call original, then rewrite new primitives.*
#include "chams.h"
#include "config.h"
#include "memory.h"
#include "debug.h"
#include <Windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <vector>

namespace Features::Visuals::Chams {

    using GeneratePrimitivesFn = void(__fastcall*)(uintptr_t a1, uintptr_t scene_object, uintptr_t scene_view, uintptr_t primitive_buffer);
    static GeneratePrimitivesFn o_GeneratePrimitives = nullptr;

    static uintptr_t g_scenesys = 0;
    static uintptr_t g_material = 0;
    static FILE* g_log = nullptr;

    static void Log(const char* fmt, ...) {
        if (!g_log) {
            char p[MAX_PATH];
            if (!GetTempPathA(MAX_PATH, p)) return;
            strcat_s(p, "vanta_chams.log");
            fopen_s(&g_log, p, "w");
        }
        if (g_log) {
            va_list ap; va_start(ap, fmt);
            vfprintf(g_log, fmt, ap);
            va_end(ap);
            fputc('\n', g_log);
            fflush(g_log);
        }
    }

    // Resolve a solid material once. Adjust index if your build differs.
    static uintptr_t ResolveMaterial() {
        HMODULE tier0 = GetModuleHandleA("tier0.dll");
        if (!tier0) return 0;
        auto create = (void* (*)(const char*, int*))GetProcAddress(tier0, "CreateInterface");
        if (!create) return 0;
        void* ms = create("VMaterialSystem2_001", nullptr);
        if (!ms) return 0;
        void** vt = *(void***)ms;
        // common FindMaterial indices
        static const int idxs[] = { 73, 74, 72, 75 };
        for (int idx : idxs) {
            auto fn = (void* (__fastcall*)(void*, const char*, const char*, int))vt[idx];
            if (!fn) continue;
            __try {
                void* m = fn(ms, "materials/dev/debug_solid.vmat", "vanta_chams", 0);
                if ((uintptr_t)m > 0x10000) {
                    Log("material resolved vt[%d] = %p", idx, m);
                    return (uintptr_t)m;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        Log("material resolve failed");
        return 0;
    }

    // Apply material + color to every newly added primitive.
    static void ApplyToNewPrimitives(uintptr_t primitive_buffer, int prev_count, uintptr_t material, uint32_t color_argb) {
        if (!primitive_buffer || !material) return;
        int new_count = Memory::Read<int>(primitive_buffer + 0xC);
        if (prev_count >= new_count) return;
        uintptr_t prims = Memory::Read<uintptr_t>(primitive_buffer);
        if (!prims) return;

        for (int i = prev_count; i < new_count; ++i) {
            uintptr_t prim = prims + (size_t)i * 0x68;
            Memory::Write<uintptr_t>(prim + 0x20, material);          // material
            Memory::Write<uint32_t>(prim + 0x50, color_argb);         // color
        }
    }

    static void __fastcall hk_GeneratePrimitives(uintptr_t a1, uintptr_t scene_object, uintptr_t scene_view, uintptr_t primitive_buffer) {
        if (!g_cfg.visuals.player.chams || !scene_object || !primitive_buffer) {
            if (o_GeneratePrimitives) o_GeneratePrimitives(a1, scene_object, scene_view, primitive_buffer);
            return;
        }

        if (!g_material) g_material = ResolveMaterial();
        if (!g_material) {
            if (o_GeneratePrimitives) o_GeneratePrimitives(a1, scene_object, scene_view, primitive_buffer);
            return;
        }

        // optional: clear a flag on the scene object so it doesn't get culled
        uint32_t flags = Memory::Read<uint32_t>(scene_object + 0x78);
        flags &= ~(1u << 3);
        Memory::Write<uint32_t>(scene_object + 0x78, flags);

        int prev = Memory::Read<int>(primitive_buffer + 0xC);

        // call original first so it fills the buffer
        if (o_GeneratePrimitives)
            o_GeneratePrimitives(a1, scene_object, scene_view, primitive_buffer);

        // then override the new primitives
        uint32_t col = 0xFF00FF00; // green for test — pull from g_cfg later
        ApplyToNewPrimitives(primitive_buffer, prev, g_material, col);

        InterlockedIncrement((volatile long*)&Debug::g_chams.fires);
    }

    bool Init() {
        Log("=== vanta chams start ===");
        g_scenesys = Memory::GetModuleBase("scenesystem.dll");
        if (!g_scenesys) {
            Log("scenesystem not found");
            return false;
        }
        Log("scenesys = %p", (void*)g_scenesys);

        // You must resolve the real GeneratePrimitives address for your build.
        // Common pattern: scenesystem.dll + offset, or via vtable of a known object.
        // Placeholder — replace 0x27DEA0 with the correct RVA from your dump or pattern scan.
        uintptr_t target = g_scenesys + 0x27DEA0;

        MH_STATUS st = MH_CreateHook((void*)target, &hk_GeneratePrimitives, (void**)&o_GeneratePrimitives);
        Log("CreateHook = %d", (int)st);
        if (st != MH_OK) return false;

        st = MH_EnableHook((void*)target);
        Log("EnableHook = %d", (int)st);
        return st == MH_OK;
    }

    void Shutdown() {
        if (g_scenesys && o_GeneratePrimitives) {
            MH_DisableHook((void*)(g_scenesys + 0x27DEA0));
        }
        if (g_log) {
            Log("shutdown");
            fclose(g_log);
            g_log = nullptr;
        }
    }
}