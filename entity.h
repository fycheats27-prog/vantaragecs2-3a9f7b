// language: C++, file: src/entity.h
#pragma once
#include <cstdint>
#include <Windows.h>

struct Vec3 { float x, y, z; };

namespace Schema {
    // ── C_BaseEntity (raíz de todo) ──
    constexpr int k_m_iHealth = 0x34C;   // int32
    constexpr int k_m_lifeState = 0x354;   // uint8
    constexpr int k_m_iTeamNum = 0x3E7;   // uint8
    constexpr int k_m_fFlags = 0x3F4;   // uint32

    // ── C_BasePlayerPawn ──
    constexpr int k_m_vOldOrigin = 0x14A4;  // Vector (origen del jugador)

    // ── CBasePlayerController ──
    constexpr int k_m_hPawn = 0x6BC;   // CHandle → C_BasePlayerPawn

    // ── C_CSPlayerPawn ──
    constexpr int k_m_angEyeAngles = 0x3600; // QAngle

    // ── CCSPlayerController ──
    constexpr int k_m_hPlayerPawn = 0x92C; // CHandle → C_CSPlayerPawn
} // namespace Schema

class C_CSPlayerPawn {
public:
    uintptr_t addr = 0;
    bool IsValid() const { return addr != 0; }

    int GetHealth() const {
        return *(int*)(addr + Schema::k_m_iHealth);
    }
    uint8_t GetTeam() const {
        return *(uint8_t*)(addr + Schema::k_m_iTeamNum);
    }
    uint8_t GetLifeState() const {
        return *(uint8_t*)(addr + Schema::k_m_lifeState);
    }
    bool IsAlive() const {
        return GetLifeState() == 0 && GetHealth() > 0;
    }

    Vec3 GetOrigin() const {
        return *(Vec3*)(addr + Schema::k_m_vOldOrigin);
    }

    Vec3 GetEyePosition() const {
        Vec3 o = GetOrigin();
        o.z += 64.0f;
        return o;
    }
};