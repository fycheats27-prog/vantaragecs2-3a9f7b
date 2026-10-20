// language: C++, file: src/entity.h
// *append or replace the class at the bottom of the file you pasted*

#include "memory.h"

class C_CSPlayerPawn {
public:
    uintptr_t addr = 0;

    bool IsValid() const {
        return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
    }

    int GetHealth() const {
        if (!IsValid()) return 0;
        return Memory::Read<int>(addr + Schema::m_iHealth);
    }
    uint8_t GetTeam() const {
        if (!IsValid()) return 0;
        return Memory::Read<uint8_t>(addr + Schema::m_iTeamNum);
    }
    uint8_t GetLifeState() const {
        if (!IsValid()) return 1;
        return Memory::Read<uint8_t>(addr + Schema::m_lifeState);
    }
    bool IsAlive() const {
        return IsValid() && GetLifeState() == 0 && GetHealth() > 0 && GetHealth() <= 100;
    }

    Vec3 GetOrigin() const {
        if (!IsValid()) return {};
        return Memory::Read<Vec3>(addr + Schema::m_vOldOrigin);
    }

    Vec3 GetBonePosition(int bone_index) const {
        if (!IsValid() || bone_index < 0 || bone_index > 32) return {};
        uintptr_t node = Memory::Read<uintptr_t>(addr + Schema::m_pGameSceneNode);
        if (node < 0x10000 || node > 0x7FFFFFFFFFFF) return {};
        uintptr_t model = Memory::Read<uintptr_t>(node + Schema::m_modelState);
        if (model < 0x10000 || model > 0x7FFFFFFFFFFF) return {};
        uintptr_t bones = Memory::Read<uintptr_t>(model + Schema::m_pBoneArray);
        if (bones < 0x10000 || bones > 0x7FFFFFFFFFFF) return {};
        return Memory::Read<Vec3>(bones + (uintptr_t)bone_index * 0x20);
    }

    Vec3 GetEyePosition() const {
        Vec3 o = GetOrigin();
        o.z += 64.f;
        return o;
    }
};