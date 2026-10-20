// --- aimbot.h ---
#pragma once
#include "config.h"

namespace Features::Rage::Aimbot {
    void Init();
    void Shutdown();
    int  GetFov();
    WeaponCategory GetCurrentCategory();
    const char* GetCurrentWeaponName();
    const char* GetCurrentCategoryName();
}