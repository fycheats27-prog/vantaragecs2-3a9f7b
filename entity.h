// language: C++, file: src/entity.h
// *Opcional si esp.cpp es autosuficiente. Reemplaza el viejo para no confundir includes.*
#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include "memory.h"

struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };
struct Vec2 { float x = 0.f, y = 0.f; };
struct Mat4 { float m[4][4]{}; };

inline bool MemReadBuf(uintptr_t addr, void* out, size_t n) {
    auto* p = static_cast<uint8_t*>(out);
    size_t off = 0;
    while (n - off >= 8) { *reinterpret_cast<uint64_t*>(p + off) = Memory::Read<uint64_t>(addr + off); off += 8; }
    while (n - off >= 4) { *reinterpret_cast<uint32_t*>(p + off) = Memory::Read<uint32_t>(addr + off); off += 4; }
    while (n - off >= 2) { *reinterpret_cast<uint16_t*>(p + off) = Memory::Read<uint16_t>(addr + off); off += 2; }
    while (n - off >= 1) { *reinterpret_cast<uint8_t*>(p + off) = Memory::Read<uint8_t>(addr + off);  off += 1; }
    return true;
}

namespace Schema {
    constexpr int k_entity_entry_size = 0x78;
    inline int m_iHealth = 0x344;
    inline int m_iTeamNum = 0x3C8;
    inline int m_lifeState = 0x354;
    inline int m_pGameSceneNode = 0x330;
    inline int m_pBoneArray = 0x1C0;
    inline int m_bDormant = 0x103;
    inline int m_hController = 0x14BC;
    inline int m_vOldOrigin = 0x14A4;
    inline int m_sSanitizedPlayerName = 0x878;
    inline int m_iPawnArmor = 0x824;
    inline int m_pClippingWeapon = 0x12A8;
    inline int m_Identity = 0x10;
    inline int m_designerName = 0x20;
    inline bool resolved = false;
    bool Resolve(uintptr_t local_pawn);
}

namespace Bones {
    enum : int {
        PELVIS = 1, SPINE_0 = 2, SPINE_1 = 3, SPINE_2 = 4,
        NECK = 6, HEAD = 7,
        CLAV_L = 8, ARM_UP_L = 9, ARM_LO_L = 10, HAND_L = 11,
        CLAV_R = 12, ARM_UP_R = 13, ARM_LO_R = 14, HAND_R = 15,
        HIP_L = 17, KNEE_L = 18, ANKLE_L = 19,
        HIP_R = 20, KNEE_R = 21, ANKLE_R = 22, CHEST = 23
    };
    constexpr int MAX = 24;
    struct Joint { Vec3 pos; char pad[0x14]; };
    static_assert(sizeof(Joint) == 0x20, "CS2 bone stride must be 0x20");
}

struct Player {
    uintptr_t controller = 0, pawn = 0;
    int index = 0, team = 0, health = 0, armor = 0;
    Vec3 origin{}, head{}, feet{};
    Vec3 bones[Bones::MAX]{};
    float distance = 0.f;
    char name[64]{}, weapon[32]{};
};

namespace World {
    bool ValidPtr(uintptr_t p);
    bool IsFinite(const Vec3& v);
    float Dist(const Vec3& a, const Vec3& b);
    bool ToScreen(const Vec3& world, Vec2& out, const Mat4& vm, int w, int h);
    uintptr_t EntityByIndex(uintptr_t list, int index);
    void DesignerName(uintptr_t entity, char* out, size_t n);
    void WeaponName(uintptr_t pawn, char* out, size_t n);
    void Sanitize(char* s, size_t n);
}

class EntityList {
public:
    void Update(uintptr_t local_pawn);
    const std::vector<Player>& Players() const { return players_; }
    int LocalTeam() const { return local_team_; }
private:
    std::vector<Player> players_;
    int local_team_ = 0;
};