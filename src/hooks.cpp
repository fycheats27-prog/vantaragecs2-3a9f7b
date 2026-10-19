// language: C++, file: src/hooks.cpp
// *Hooks centralizados: Present, CreateMove, FirstPerson, ThirdPerson.*

#include "hooks.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include "present.h"
#include "createmove.h"
#include "thirdperson.h"
#include <Windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cstdio>

namespace Hooks {

    static constexpr uintptr_t kCreateMoveRVA = 0xB65A70;
    static constexpr uintptr_t kFirstPersonRVA = 0xB63D40;
    static constexpr uintptr_t kThirdPersonRVA = 0xB63E20;
    static constexpr uintptr_t kForceAttackRVA = 0x2233ED0;

    static constexpr uintptr_t kUserCmdOffset = 0x5540;

    using CreateMoveFn = void(__fastcall*)(void* pInput, int slot, char active);
    using ConCommandFn = __int64(__fastcall*)();

    static CreateMoveFn o_CreateMove = nullptr;
    static ConCommandFn o_FirstPerson = nullptr;
    static ConCommandFn o_ThirdPerson = nullptr;

    static uintptr_t g_client = 0;

    static void __fastcall hk_CreateMove(void* pInput, int slot, char active) {
        if (pInput) {
            // 1) Third person flags
            Features::Visuals::ThirdPerson::Apply(pInput);

            // 2) Bhop logic
            void* pCmd = *reinterpret_cast<void**>((uintptr_t)pInput + kUserCmdOffset);
            if (pCmd) {
                CreateMove::Apply(pCmd);
            }

            // 3) Forzar disparo (dwForceAttack)
            if (g_cfg.visuals.player.third_person && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                *(int*)(g_client + kForceAttackRVA) = 65537; // IN_ATTACK
            }
            else {
                // No tocar si el usuario no está en third person
                // (El juego maneja el clic izquierdo normalmente)
            }
        }
        o_CreateMove(pInput, slot, active);
    }

    static __int64 __fastcall hk_FirstPerson() {
        if (g_cfg.visuals.player.third_person) return 0;
        return o_FirstPerson();
    }

    static __int64 __fastcall hk_ThirdPerson() {
        if (g_cfg.visuals.player.third_person) return 0;
        return o_ThirdPerson();
    }

    bool Init() {
        if (MH_Initialize() != MH_OK) return false;

        g_client = Memory::GetModuleBase("client.dll");
        if (!g_client) return false;

        // 1) Present (ImGui)
        if (!Present::Init()) {
            MessageBoxA(nullptr, "Present Init fallo", "vanta", MB_OK);
            return false;
        }

        // 2) CreateMove (Un solo hook)
        uintptr_t cm_addr = g_client + kCreateMoveRVA;
        if (MH_CreateHook((LPVOID)cm_addr, &hk_CreateMove, reinterpret_cast<LPVOID*>(&o_CreateMove)) == MH_OK) {
            MH_EnableHook((LPVOID)cm_addr);
        }

        // 3) Bypasses de cámara
        MH_CreateHook((LPVOID)(g_client + kFirstPersonRVA), &hk_FirstPerson, reinterpret_cast<LPVOID*>(&o_FirstPerson));
        MH_EnableHook((LPVOID)(g_client + kFirstPersonRVA));

        MH_CreateHook((LPVOID)(g_client + kThirdPersonRVA), &hk_ThirdPerson, reinterpret_cast<LPVOID*>(&o_ThirdPerson));
        MH_EnableHook((LPVOID)(g_client + kThirdPersonRVA));

        return true;
    }

    void Shutdown() {
        MH_DisableHook(MH_ALL_HOOKS);
        Present::Shutdown();
        MH_Uninitialize();
    }
}