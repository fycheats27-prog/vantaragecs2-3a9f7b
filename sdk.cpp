// language: C++, file: src/sdk.cpp

#include "sdk.h"
#include "memory.h"

namespace SDK {

    Offsets_t g_off;

    bool Init() {
        if (!Memory::Init()) return false;
        return g_off.dwLocalPlayerPawn != 0 && g_off.dwEntityList != 0;
    }

    static uintptr_t ClientBase() {
        return Memory::GetModuleBase("client.dll");
    }

    uintptr_t GetEntityList() {
        uintptr_t base = ClientBase();
        if (!base) return 0;
        return Memory::Read<uintptr_t>(base + g_off.dwEntityList);
    }

    uintptr_t GetEntityFromHandle(uint32_t handle) {
        if (handle == 0xFFFFFFFF) return 0;
        uintptr_t list = GetEntityList();
        if (!list) return 0;

        uint32_t index = handle & 0x7FFF;
        uintptr_t chunk = Memory::Read<uintptr_t>(list + 0x8 * (index >> 9) + 0x10);
        if (!chunk) return 0;
        return Memory::Read<uintptr_t>(chunk + 0x78 * (index & 0x1FF));
    }

    uintptr_t GetLocalPlayerPawn() {
        uintptr_t base = ClientBase();
        if (!base) return 0;
        return Memory::Read<uintptr_t>(base + g_off.dwLocalPlayerPawn);
    }

} // namespace SDK