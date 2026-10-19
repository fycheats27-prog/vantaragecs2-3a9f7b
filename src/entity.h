// language: C++, file: src/entity.h
#pragma once
#include <cstdint>
#include <Windows.h>

struct Vec3 { float x, y, z; };

namespace Schema {
    // Offsets actualizados según el volcado más reciente (Build 14184)
    constexpr int k_m_iHealth = 0x344;   // int
    constexpr int k_m_iTeamNum = 0x3C8;   // uint8
    constexpr int k_m_lifeState = 0x338;   // uint8 (0 = alive)
    constexpr int k_m_vecOrigin = 0x1224;  // Vec3
    constexpr int k_m_pBoneArray = 0x1F00;  // ptr a matrix3x4_t[]
    constexpr int k_m_fFlags = 0x3F8;   // uint32, bit 0 = on ground
    constexpr int k_m_hPlayerPawn = 0x2348;  // handle (desde controller)
} // namespace Schema

class C_CSPlayerPawn {
public:
    uintptr_t addr = 0;
    bool IsValid() const { return addr != 0; }

    int GetHealth() const { return *(int*)(addr + Schema::k_m_iHealth); }
    uint8_t GetTeam() const { return *(uint8_t*)(addr + Schema::k_m_iTeamNum); }
    uint8_t GetLifeState() const { return *(uint8_t*)(addr + Schema::k_m_lifeState); }
    bool IsAlive() const { return GetLifeState() == 0 && GetHealth() > 0; }

    Vec3 GetOrigin() const { return *(Vec3*)(addr + Schema::k_m_vecOrigin); }

    Vec3 GetBonePosition(int bone_index) const {
        uintptr_t bones = *(uintptr_t*)(addr + Schema::k_m_pBoneArray);
        if (!bones) return {};
        float* m = (float*)(bones + bone_index * 0x30);
        return { m[3], m[7], m[11] };
    }

    Vec3 GetEyePosition() const {
        Vec3 o = GetOrigin();
        o.z += 64.0f;
        return o;
    }
};