// --- hooks.cpp ---
#include "hooks.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include "present.h"
#include "createmove.h"
#include "chams.h"
#include "debug.h"
#include <Windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cmath>

namespace Hooks {

    static constexpr uintptr_t kCreateMoveRVA = 0xB65A70;
    static constexpr uintptr_t kForceAttackRVA = 0x2233ED0;
    static constexpr uintptr_t kUserCmdOffset = 0x5540;
    static constexpr uintptr_t kOverrideViewRVA = 0xD02E30;

    struct Vec3 { float x, y, z; };
    struct CViewSetup {
        char pad_0000[0x4A0];
        Vec3 vecOrigin;
        char pad_04AC[0xC];
        Vec3 angView;
    };

    using CreateMoveFn = void(__fastcall*)(void*, int, char);
    using OverrideViewFn = void(__fastcall*)(__int64, CViewSetup*);

    static CreateMoveFn   o_CreateMove = nullptr;
    static OverrideViewFn o_OverrideView = nullptr;
    static uintptr_t g_client = 0;

    static void __fastcall hk_CreateMove(void* pInput, int slot, char active) {
        Debug::g_tp.menu_on = g_cfg.visuals.player.third_person ? 1 : 0;
        if (pInput) {
            void* pCmd = *reinterpret_cast<void**>((uintptr_t)pInput + kUserCmdOffset);
            if (pCmd) CreateMove::Apply(pCmd);
            if (g_cfg.visuals.player.third_person && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                *(int*)(g_client + kForceAttackRVA) = 65537;
        }
        if (o_CreateMove) o_CreateMove(pInput, slot, active);
    }

    static void __fastcall hk_OverrideView(__int64 a1, CViewSetup* pSetup) {
        Debug::g_tp.ticks++;
        if (o_OverrideView) o_OverrideView(a1, pSetup);
        if (!pSetup || !g_cfg.visuals.player.third_person) return;

        float dist = g_cfg.visuals.player.third_person_dist;
        if (dist < 10.f || dist > 500.f) dist = 100.f;

        float rx = pSetup->angView.x * 0.01745329251f;
        float ry = pSetup->angView.y * 0.01745329251f;
        float fx = cosf(rx) * cosf(ry);
        float fy = cosf(rx) * sinf(ry);
        float fz = -sinf(rx);

        pSetup->vecOrigin.x -= fx * dist;
        pSetup->vecOrigin.y -= fy * dist;
        pSetup->vecOrigin.z -= fz * dist;
    }

    bool Init() {
        if (MH_Initialize() != MH_OK) return false;
        g_client = Memory::GetModuleBase("client.dll");
        if (!g_client) return false;
        if (!Present::Init()) return false;

        uintptr_t cm = g_client + kCreateMoveRVA;
        if (MH_CreateHook((LPVOID)cm, &hk_CreateMove, (LPVOID*)&o_CreateMove) == MH_OK)
            MH_EnableHook((LPVOID)cm);

        uintptr_t ov = g_client + kOverrideViewRVA;
        if (MH_CreateHook((LPVOID)ov, &hk_OverrideView, (LPVOID*)&o_OverrideView) == MH_OK)
            MH_EnableHook((LPVOID)ov);

        Features::Visuals::Chams::Init();
        return true;
    }

    void Shutdown() {
        Features::Visuals::Chams::Shutdown();
        MH_DisableHook(MH_ALL_HOOKS);
        Present::Shutdown();
        MH_Uninitialize();
    }
}