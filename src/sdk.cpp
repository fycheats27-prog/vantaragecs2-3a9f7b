// language: C++, file: src/sdk.cpp
#include "sdk.h"
#include "memory.h"

namespace SDK {

    Offsets_t g_off;

    bool Init() {
        // Ya no se escanean patrones. Solo se resuelve la base de client.dll.
        // Los offsets son fijos para el build actual y se leen directamente.
        if (!Memory::Init()) return false;

        // Opcional: verificar que los offsets no son cero.
        // Si el juego se actualiza, estos valores deberán ser reemplazados.
        return g_off.dwEntityList != 0 && g_off.dwLocalPlayerPawn != 0;
    }

    uintptr_t GetEntityList() {
        if (!g_off.dwEntityList) return 0;
        return Memory::Read<uintptr_t>(Memory::GetModuleBase("client.dll") + g_off.dwEntityList);
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
        if (!g_off.dwLocalPlayerPawn) return 0;
        return Memory::Read<uintptr_t>(Memory::GetModuleBase("client.dll") + g_off.dwLocalPlayerPawn);
    }

} // namespace SDK