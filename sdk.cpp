// --- sdk.cpp ---
#include "sdk.h"
#include "memory.h"
#include "entity.h"

namespace SDK {

    bool Init() {
        return Memory::Init();
    }

    uintptr_t GetEntityList() {
        const uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return 0;
        return Memory::Read<uintptr_t>(client + Offsets::dwEntityList);
    }

    uintptr_t GetEntityFromHandle(uint32_t handle) {
        if (handle == 0xFFFFFFFF || handle == 0) return 0;
        const uintptr_t list = GetEntityList();
        if (!list) return 0;
        const uint32_t index = handle & 0x7FFF;
        const uintptr_t chunk = Memory::Read<uintptr_t>(list + 0x10 + (index >> 9) * 8);
        if (!chunk) return 0;
        return Memory::Read<uintptr_t>(chunk + (index & 0x1FF) * 0x78);
    }

    uintptr_t GetLocalPlayerPawn() {
        const uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return 0;
        return Memory::Read<uintptr_t>(client + Offsets::dwLocalPlayerPawn);
    }

    uintptr_t GetLocalPlayerController() {
        const uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return 0;
        return Memory::Read<uintptr_t>(client + Offsets::dwLocalPlayerController);
    }

    // bone array: pawn → m_pGameSceneNode → +0x140 (modelState) → +0x80 (bone ptr)
    uintptr_t GetBoneArray(uintptr_t pawn) {
        if (!pawn) return 0;
        const uintptr_t node = Memory::Read<uintptr_t>(pawn + Schema::m_pGameSceneNode);
        if (!node) return 0;
        return Memory::Read<uintptr_t>(node + Schema::m_modelState + Schema::m_pBoneArray);
    }

} // namespace SDK