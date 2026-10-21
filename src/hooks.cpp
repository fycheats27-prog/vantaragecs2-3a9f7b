#include "hooks.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include "present.h"
#include "createmove.h"
#include "bhop.h"
#include "misc.h"
#include "chams.h"
#include "aimbot.h"
#include "triggerbot.h"
#include "debug.h"
#include <Windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cmath>

namespace Hooks {

    static constexpr uintptr_t kOverrideViewRVA = 0xD02E30;

    struct Vec3 { float x, y, z; };
    struct CViewSetup {
        char pad_0000[0x4A0];
        Vec3 vecOrigin;
        char pad_04AC[0xC];
        Vec3 angView;
    };

    using OverrideViewFn = void(__fastcall*)(__int64 a1, CViewSetup* pSetup);

    static OverrideViewFn o_OverrideView = nullptr;
    static uintptr_t g_client = 0;

    static void __fastcall hk_OverrideView(__int64 a1, CViewSetup* pSetup) {
        Debug::g_tp.ticks++;
        if (o_OverrideView) o_OverrideView(a1, pSetup);
        if (!pSetup || !g_cfg.visuals.player.third_person) return;

        float dist = g_cfg.visuals.player.third_person_dist;
        if (dist < 10.f || dist > 500.f) dist = 100.f;

        Vec3 forward;
        float rx = pSetup->angView.x * 0.01745329251f;
        float ry = pSetup->angView.y * 0.01745329251f;
        forward.x = cos(rx) * cos(ry);
        forward.y = cos(rx) * sin(ry);
        forward.z = -sin(rx);

        Vec3 eyePos = pSetup->vecOrigin;
        pSetup->vecOrigin.x = eyePos.x - (forward.x * dist);
        pSetup->vecOrigin.y = eyePos.y - (forward.y * dist);
        pSetup->vecOrigin.z = eyePos.z - (forward.z * dist);
    }

    bool Init() {
        if (MH_Initialize() != MH_OK) return false;
        g_client = Memory::GetModuleBase("client.dll");
        if (!g_client) return false;
        if (!Present::Init()) return false;

        // Hook OverrideView (Third Person)
        uintptr_t ov = g_client + kOverrideViewRVA;
        Debug::g_tp.v250 = *(uint8_t*)ov;
        Debug::g_tp.v228 = *(uint8_t*)(ov + 1);
        if (MH_CreateHook((LPVOID)ov, &hk_OverrideView, reinterpret_cast<LPVOID*>(&o_OverrideView)) == MH_OK)
            MH_EnableHook((LPVOID)ov);

        // Hook CreateMove (Bhop, Fast-Stop, Counter-Strafe)
        Hooks::CreateMove::Init();

        // Iniciar Aimbot y Triggerbot
        Features::Rage::Aimbot::Init();
        Features::Rage::Triggerbot::Init();

        return true;
    }

    void Shutdown() {
        Features::Rage::Triggerbot::Shutdown();
        Features::Rage::Aimbot::Shutdown();
        Hooks::CreateMove::Shutdown();
        MH_DisableHook(MH_ALL_HOOKS);
        Present::Shutdown();
        MH_Uninitialize();
    }
}