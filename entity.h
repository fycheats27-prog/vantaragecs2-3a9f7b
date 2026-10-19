// --- entity.h ---
#pragma once
#include <cstdint>
#include <Windows.h>

struct Vec3 { float x, y, z; };

namespace Schema {
    // ── C_BaseEntity ──────────────────────────────────────
    constexpr int m_iHealth = 0x34C;
    constexpr int m_iMaxHealth = 0x348;
    constexpr int m_iTeamNum = 0x3E7;
    constexpr int m_lifeState = 0x354;
    constexpr int m_fFlags = 0x3F4;
    constexpr int m_pGameSceneNode = 0x330;
    constexpr int m_vecAbsVelocity = 0x3F8;

    // ── CGameSceneNode ────────────────────────────────────
    constexpr int m_vecAbsOrigin = 0xC8;   // dentro del node
    constexpr int m_bDormant = 0x103;

    // ── CSkeletonInstance (extends CGameSceneNode) ────────
    // m_modelState = 0x140; bone array pointer lives at +0x80 inside modelState
    constexpr int m_modelState = 0x140;
    constexpr int m_pBoneArray = 0x80;   // relative to modelState base

    // ── C_BasePlayerPawn ──────────────────────────────────
    constexpr int m_vOldOrigin = 0x14A4;
    constexpr int m_hController = 0x14BC;
    constexpr int m_pWeaponServices = 0x12F0;
    constexpr int m_pCameraServices = 0x1328;

    // ── C_CSPlayerPawn ────────────────────────────────────
    constexpr int m_ArmorValue = 0x1EC8;
    constexpr int m_angEyeAngles = 0x3600;
    constexpr int m_bIsScoped = 0x1E98;
    constexpr int m_iShotsFired = 0x1EB0;
    constexpr int m_flVelocityModifier = 0x1EB8;
    constexpr int m_entitySpottedState = 0x1E80;   // EntitySpottedState_t
    constexpr int m_bSpottedByMask = 0xC;        // inside EntitySpottedState_t

    // ── CCSPlayerController ───────────────────────────────
    constexpr int m_hPlayerPawn = 0x92C;
    constexpr int m_bPawnIsAlive = 0x934;
    constexpr int m_iPawnHealth = 0x938;
    constexpr int m_iPawnArmor = 0x93C;
    constexpr int m_sSanitizedPlayerName = 0x878;

    // ── CPlayer_WeaponServices ────────────────────────────
    constexpr int m_hActiveWeapon = 0x60;
    constexpr int m_hMyWeapons = 0x48;

    // ── C_PlantedC4 ───────────────────────────────────────
    constexpr int m_bBombTicking = 0x1288;
    constexpr int m_flC4Blow = 0x12B8;
    constexpr int m_nBombSite = 0x128C;
    constexpr int m_flDefuseCountDown = 0x12D8;
    constexpr int m_hBombDefuser = 0x12E0;
    constexpr int m_bBeingDefused = 0x12C4;
    constexpr int m_flDefuseLength = 0x12D4;
    constexpr int m_bBombDefused = 0x12DC;

    // ── C_CSWeaponBase ────────────────────────────────────
    constexpr int m_bInReload = 0x1A3C;
    constexpr int m_fLastShotTime = 0x1B68;

    // ── Entity identity (for designer name) ───────────────
    constexpr int m_Identity = 0x10;
    constexpr int m_designerName = 0x20;
} // namespace Schema

// ── client.dll globals ────────────────────────────────────────────
namespace Offsets {
    constexpr uintptr_t dwEntityList = 0x271A908;
    constexpr uintptr_t dwLocalPlayerPawn = 0x2565718;
    constexpr uintptr_t dwLocalPlayerController = 0x253CB78;
    constexpr uintptr_t dwViewMatrix = 0x256B020;
    constexpr uintptr_t dwViewAngles = 0x257BCC8;
    constexpr uintptr_t dwGlobalVars = 0x2230090;
    constexpr uintptr_t dwGameRules = 0x25651D0;
    constexpr uintptr_t dwPlantedC4 = 0x24CB3C0;
    constexpr uintptr_t dwGlowManager = 0x2561EF0;
    constexpr uintptr_t dwCSGOInput = 0x257B640;
    // ── engine2.dll ──
    constexpr uintptr_t dwWindowWidth = 0x91F338;
    constexpr uintptr_t dwWindowHeight = 0x91F33C;
    constexpr uintptr_t dwNetworkGameClient = 0x91AFC0;
    constexpr uintptr_t dwBuildNumber = 0x61CFEC;
} // namespace Offsets