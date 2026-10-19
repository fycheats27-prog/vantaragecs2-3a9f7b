#pragma once
#include <cstdint>

namespace SDK {

    struct Offsets_t {
        uintptr_t dwEntityList      = 0x271A908;
        uintptr_t dwLocalPlayerPawn = 0x2565718;
        uintptr_t dwViewAngles      = 0x257BCC8;
        uintptr_t dwViewMatrix      = 0x256B020;
        uintptr_t dwCSGOInput       = 0x257B640;
    };

    extern Offsets_t g_off;

    bool Init();

    uintptr_t GetEntityList();
    uintptr_t GetEntityFromHandle(uint32_t handle);
    uintptr_t GetLocalPlayerPawn();

} // namespace SDK
