// language: C++, file: src/entity.cpp
#include "entity.h"
#include "sdk.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cmath>
#include <algorithm>

bool World::ValidPtr(uintptr_t p) {
    return p > 0x10000 && p < 0x7FFFFFFFFFFF;
}

bool World::IsFinite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
        && !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
}

float World::Dist(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool World::ToScreen(const Vec3& world, Vec2& out, const Mat4& vm, int w, int h) {
    const float cw = vm.m[3][0] * world.x + vm.m[3][1] * world.y + vm.m[3][2] * world.z + vm.m[3][3];
    if (!std::isfinite(cw) || cw < 0.001f) return false;
    const float cx = vm.m[0][0] * world.x + vm.m[0][1] * world.y + vm.m[0][2] * world.z + vm.m[0][3];
    const float cy = vm.m[1][0] * world.x + vm.m[1][1] * world.y + vm.m[1][2] * world.z + vm.m[1][3];
    const float inv = 1.f / cw;
    out.x = (w * 0.5f) * (cx * inv + 1.f);
    out.y = (h * 0.5f) * (1.f - cy * inv);
    return std::isfinite(out.x) && std::isfinite(out.y);
}

uintptr_t World::EntityByIndex(uintptr_t list, int index) {
    if (!ValidPtr(list)) return 0;
    const uintptr_t chunk = Memory::Read<uintptr_t>(list + 0x8ull * (index >> 9) + 0x10);
    if (!ValidPtr(chunk)) return 0;
    return Memory::Read<uintptr_t>(chunk + (uintptr_t)Schema::k_entity_entry_size * (uintptr_t)(index & 0x1FF));
}

void World::DesignerName(uintptr_t entity, char* out, size_t n) {
    if (!out || !n) return;
    out[0] = 0;
    if (!ValidPtr(entity)) return;
    const uintptr_t identity = Memory::Read<uintptr_t>(entity + Schema::m_Identity);
    if (!ValidPtr(identity)) return;
    const uintptr_t str = Memory::Read<uintptr_t>(identity + Schema::m_designerName);
    if (!ValidPtr(str)) return;
    MemReadBuf(str, out, n - 1);
    out[n - 1] = 0;
}

void World::WeaponName(uintptr_t pawn, char* out, size_t n) {
    if (!out || !n) return;
    out[0] = 0;
    const uintptr_t w = Memory::Read<uintptr_t>(pawn + Schema::m_pClippingWeapon);
    if (!ValidPtr(w)) return;

    char d[64]{};
    DesignerName(w, d, sizeof(d));
    if (std::strncmp(d, "weapon_", 7) != 0 &&
        std::strncmp(d, "knife", 5) != 0 &&
        std::strncmp(d, "c4", 2) != 0) return;

    const char* p = d;
    if (std::strncmp(p, "weapon_", 7) == 0) p += 7;

    struct Map { const char* in; const char* out; };
    static const Map map[] = {
        {"ak47","AK-47"}, {"m4a1","M4A1"}, {"m4a1_silencer","M4A1-S"},
        {"awp","AWP"}, {"deagle","Deagle"}, {"glock","Glock"},
        {"usp_silencer","USP-S"}, {"p250","P250"}, {"famas","FAMAS"},
        {"galilar","Galil"}, {"sg556","SG553"}, {"aug","AUG"},
        {"ssg08","SSG08"}, {"mp9","MP9"}, {"mac10","MAC-10"},
        {"mp7","MP7"}, {"ump45","UMP-45"}, {"p90","P90"},
        {"bizon","Bizon"}, {"nova","Nova"}, {"xm1014","XM1014"},
        {"mag7","MAG-7"}, {"sawedoff","Sawed-Off"}, {"m249","M249"},
        {"negev","Negev"}, {"taser","Zeus"}, {"knife","Knife"}, {"c4","C4"}
    };
    for (const auto& m : map)
        if (std::strcmp(p, m.in) == 0) { std::snprintf(out, n, "%s", m.out); return; }

    size_t i = 0;
    for (; p[i] && i + 1 < n; ++i)
        out[i] = (char)std::toupper((unsigned char)p[i]);
    out[i] = 0;
}

void World::Sanitize(char* s, size_t n) {
    if (!s || !n) return;
    s[n - 1] = 0;
    for (size_t i = 0; s[i]; ++i)
        if ((unsigned char)s[i] < 0x20) s[i] = ' ';
}

bool Schema::Resolve(uintptr_t pawn) {
    const auto ok = [](uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFF; };

    const int hp = Memory::Read<int>(pawn + m_iHealth);

    for (int c : { 0x3E3, 0x3C8, 0x3DF, 0x3E0 }) {
        const int v = Memory::Read<uint8_t>(pawn + c);
        if (v == 2 || v == 3) { m_iTeamNum = c; break; }
    }

    for (int c : { 0x348, 0x354, 0x338 }) {
        const int v = Memory::Read<uint8_t>(pawn + c);
        const bool plausible = (hp > 0) ? (v == 0) : (v <= 2);
        if (plausible) { m_lifeState = c; break; }
    }

    bool got_node = false;
    for (int n : { 0x330, 0x328, 0x310 }) {
        const uintptr_t node = Memory::Read<uintptr_t>(pawn + n);
        if (!ok(node)) continue;
        for (int b : { 0x1C0, 0x1E0 }) {
            const uintptr_t arr = Memory::Read<uintptr_t>(node + b);
            if (!ok(arr)) continue;
            Bones::Joint j{};
            MemReadBuf(arr + (uintptr_t)Bones::HEAD * sizeof(Bones::Joint), &j, sizeof(j));
            if (std::isfinite(j.pos.x) && std::fabs(j.pos.x) < 20000.f && std::fabs(j.pos.z) < 5000.f) {
                m_pGameSceneNode = n;
                m_pBoneArray = b;
                got_node = true;
                break;
            }
        }
        if (got_node) break;
    }

    for (int c : { 0x14A4, 0x1224 }) {
        const Vec3 o = Memory::Read<Vec3>(pawn + c);
        if (std::isfinite(o.x) && std::fabs(o.x) < 20000.f && std::fabs(o.y) < 20000.f &&
            o.z > -2048.f && o.z < 8192.f) {
            m_vOldOrigin = c; break;
        }
    }

    uintptr_t ctrl = 0;
    for (int c : { 0x14BC, 0x14C0, 0x14B8 }) {
        const uint32_t h = Memory::Read<uint32_t>(pawn + c);
        if (h == 0 || h == 0xFFFFFFFF) continue;
        const uintptr_t e = SDK::GetEntityFromHandle(h);
        if (ok(e)) { m_hController = c; ctrl = e; break; }
    }

    if (ok(ctrl)) {
        for (int c : { 0x878, 0x870, 0x868 }) {
            char b = 0;
            MemReadBuf(ctrl + c, &b, 1);
            if (b == 0 || (b >= 0x20 && b < 0x7F)) { m_sSanitizedPlayerName = c; break; }
        }
    }

    resolved = got_node;

    char log[256];
    std::snprintf(log, sizeof(log),
        "[vanta] schema resolved=%d team=0x%X life=0x%X node=0x%X bones=0x%X origin=0x%X ctrl=0x%X name=0x%X\n",
        (int)resolved, m_iTeamNum, m_lifeState, m_pGameSceneNode, m_pBoneArray,
        m_vOldOrigin, m_hController, m_sSanitizedPlayerName);
    OutputDebugStringA(log);
    return resolved;
}

void EntityList::Update(uintptr_t local_pawn) {
    players_.clear();
    players_.reserve(16);
    local_team_ = 0;

    const uintptr_t list = SDK::GetEntityList();
    if (!World::ValidPtr(list)) return;

    if (!Schema::resolved && World::ValidPtr(local_pawn))
        Schema::Resolve(local_pawn);

    Vec3 local_origin{};
    if (World::ValidPtr(local_pawn)) {
        local_origin = Memory::Read<Vec3>(local_pawn + Schema::m_vOldOrigin);
        local_team_ = Memory::Read<uint8_t>(local_pawn + Schema::m_iTeamNum);
    }

    for (int i = 1; i <= 256; ++i) {
        const uintptr_t ent = World::EntityByIndex(list, i);
        if (!World::ValidPtr(ent) || ent == local_pawn) continue;

        const uint32_t h_ctrl = Memory::Read<uint32_t>(ent + Schema::m_hController);
        if (h_ctrl == 0 || h_ctrl == 0xFFFFFFFF) continue;
        const uintptr_t ctrl = SDK::GetEntityFromHandle(h_ctrl);
        if (!World::ValidPtr(ctrl)) continue;

        char cname[64]{};
        World::DesignerName(ctrl, cname, sizeof(cname));
        if (!std::strstr(cname, "player_controller")) continue;

        const int team = Memory::Read<uint8_t>(ctrl + Schema::m_iTeamNum);
        if (team != 2 && team != 3) continue;

        const int hp = Memory::Read<int>(ent + Schema::m_iHealth);
        if (hp <= 0 || hp > 100) continue;
        if (Memory::Read<uint8_t>(ent + Schema::m_lifeState) != 0) continue;

        const uintptr_t node = Memory::Read<uintptr_t>(ent + Schema::m_pGameSceneNode);
        if (!World::ValidPtr(node)) continue;
        if (Memory::Read<bool>(node + Schema::m_bDormant)) continue;

        const uintptr_t bone_arr = Memory::Read<uintptr_t>(node + Schema::m_pBoneArray);
        if (!World::ValidPtr(bone_arr)) continue;

        Bones::Joint joints[Bones::MAX]{};
        MemReadBuf(bone_arr, joints, sizeof(joints));
        if (!World::IsFinite(joints[Bones::HEAD].pos)) continue;

        Player p{};
        p.pawn = ent;
        p.controller = ctrl;
        p.index = i;
        p.team = team;
        p.health = hp;
        p.armor = std::clamp(Memory::Read<int>(ent + Schema::m_iPawnArmor), 0, 100);
        p.origin = Memory::Read<Vec3>(ent + Schema::m_vOldOrigin);
        if (!World::IsFinite(p.origin)) continue;

        for (int b = 0; b < Bones::MAX; ++b) p.bones[b] = joints[b].pos;

        p.head = joints[Bones::HEAD].pos;
        p.head.z += 6.f;

        const Vec3 a = joints[Bones::ANKLE_L].pos;
        const Vec3 c = joints[Bones::ANKLE_R].pos;
        const bool va = World::IsFinite(a);
        const bool vc = World::IsFinite(c);
        if (va && vc)      p.feet = (a.z < c.z) ? a : c;
        else if (va)       p.feet = a;
        else if (vc)       p.feet = c;
        else { p.feet = joints[Bones::PELVIS].pos; p.feet.z -= 34.f; }
        p.feet.z -= 2.f;

        MemReadBuf(ctrl + Schema::m_sSanitizedPlayerName, p.name, sizeof(p.name) - 1);
        World::Sanitize(p.name, sizeof(p.name));
        World::WeaponName(ent, p.weapon, sizeof(p.weapon));

        p.distance = World::Dist(local_origin, p.origin);
        players_.push_back(p);
    }
}