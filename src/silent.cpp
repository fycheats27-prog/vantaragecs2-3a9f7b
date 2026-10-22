// silent.cpp — fixed
//
// Bugs vs. el archivo que funciona (aim.h / aim_common.h):
//
// 1) VIEW ANGLE WRITE (el bug principal):
//    Antes:  *(float*)(cmd + 0x40) = ang
//    En CS2 +0x40 NO es el PB view angle. El campo real vive en:
//      cmd->csgoUserCmd.pBaseCmd->pViewAngles->angValue
//    y el sub-tick del tick actual necesita:
//      cmd->SetSubTickAngle(ang)
//    Sin esto el servidor nunca vio el ángulo — la cámara renderizaba el
//    ángulo viejo y el disparo salía por donde miraba el ratón.
//
// 2) ATTACK HISTORY (por qué no pegaba aunque el ángulo llegara):
//    En CS2 el rayo del disparo se registra desde el INPUT HISTORY, no desde
//    el base cmd. Hay que:
//      cmd->csgoUserCmd.SetAttack1StartHistoryIndex(idx)
//      cmd->SetAttackHistoryFire(idx, ang, eye, /*narrow=*/true)
//    narrow=true es el "silent": escribe SOLO en la entrada de historial,
//    deja el base cmd intacto (cámara no se mueve).
//
// 3) AUTO-FIRE BUTTONS:
//    Antes:  *(uint64_t*)(cmd + 0x00) |= IN_ATTACK  — escribía sobre la vtable.
//    Ahora:  cmd->nButtons.nValue / nValueChanged + base->pInButtonState.
//
// 4) ENTITY WALK + LECTURA DE HUESOS: se quedan igual. Están bien
//    (controller→pawn por m_hPlayerPawn; bone array en node+0x140+0x80).
//
// Los offsets dwEntityList / dwLocalPlayerPawn / dwViewAngles son por build.
// Re-derivar contra el schema actual de client.dll. El fix de write-path
// hace que el aim funcione; offsets viejos siguen rompiéndolo.
//
// Necesita los headers del proyecto que ya funcionan:
//   #include "CUserCmd.h"    (CUserCmd, CBaseUserCmdPB, CMsgQAngle, CInButtonState)
//   #include "CCSGOInput.h"  (Input::GetCSGOInput)
// Ajusta las rutas al árbol de tu proyecto.

#pragma comment(lib, "minhook.lib")

#include "silent.h"
#include "config.h"
#include "memory.h"
#include "debug.h"
#include "CUserCmd.h"
#include "CCSGOInput.h"
#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstring>

namespace Features::Rage::SilentAim {

    struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };

    static constexpr uintptr_t dwEntityList = 0x271A908;
    static constexpr uintptr_t dwLocalPlayerPawn = 0x2565718;
    static constexpr uintptr_t dwViewAngles = 0x257BCC8;

    static constexpr int m_iHealth = 0x34C;
    static constexpr int m_iTeamNum = 0x3E7;
    static constexpr int m_lifeState = 0x354;
    static constexpr int m_pGameSceneNode = 0x330;
    static constexpr int m_vOldOrigin = 0x14A4;
    static constexpr int m_vecAbsVelocity = 0x3F8;
    static constexpr int m_hPlayerPawn = 0x92C;
    static constexpr int m_modelState = 0x140;
    static constexpr int m_pBoneArray = 0x80;
    static constexpr int m_bDormant = 0x103;
    static constexpr int m_pWeaponServices = 0x12F0;
    static constexpr int m_hActiveWeapon = 0x60;
    static constexpr int m_Identity = 0x10;
    static constexpr int m_designerName = 0x20;

    // Botones CCSGOInput (Input::GetCSGOInput). Del archivo que funciona:
    // +0x250 = down, +0x258 = pressed (edge), +0x260/+0x268 = otros estados.
    // Solo usamos +0x258 para el rising edge del semi.
    constexpr uintptr_t kCsgoBtn0 = 0x250;
    constexpr uintptr_t kCsgoBtn1 = 0x258;
    constexpr uintptr_t kCsgoBtn2 = 0x260;
    constexpr uintptr_t kCsgoBtn3 = 0x268;

    static std::atomic<bool> g_running{ false };

    // ── Helpers ──────────────────────────────────────────────────────────
    template<typename T>
    static T SafeRead(uintptr_t addr) {
        if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T{};
        __try { return *reinterpret_cast<T*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
    }

    static float NormalizeAngle(float a) {
        while (a > 180.f)  a -= 360.f;
        while (a < -180.f) a += 360.f;
        return a;
    }

    static Vec3 CalcAngle(const Vec3& src, const Vec3& dst) {
        Vec3 d{ dst.x - src.x, dst.y - src.y, dst.z - src.z };
        float hyp = sqrtf(d.x * d.x + d.y * d.y);
        return { atan2f(-d.z, hyp) * 57.29577951308232f,
                 atan2f(d.y, d.x) * 57.29577951308232f,
                 0.f };
    }

    static float Dist(const Vec3& a, const Vec3& b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    static bool IsHotkeyDown(int key) {
        if (key == 0) return false;
        if (GetAsyncKeyState(key) & 0x8000) return true;
        if (key > 0xFF) {
            int masked = key & 0xFF;
            if (masked && (GetAsyncKeyState(masked) & 0x8000)) return true;
        }
        UINT vk = MapVirtualKey((UINT)key, MAPVK_VSC_TO_VK);
        if (vk && vk != (UINT)key && (GetAsyncKeyState(vk) & 0x8000)) return true;
        return false;
    }

    static void ReadDesigner(uintptr_t ent, char* out, size_t n) {
        if (!out || !n) return;
        out[0] = 0;
        uintptr_t id = SafeRead<uintptr_t>(ent + m_Identity);
        if (!id) return;
        uintptr_t str = SafeRead<uintptr_t>(id + m_designerName);
        if (!str) return;
        __try { memcpy(out, reinterpret_cast<void*>(str), n - 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; return; }
        out[n - 1] = 0;
    }

    static uintptr_t GetActiveWeapon(uintptr_t client, uintptr_t pawn) {
        uintptr_t ws = SafeRead<uintptr_t>(pawn + m_pWeaponServices);
        if (!ws) return 0;
        uint32_t h = SafeRead<uint32_t>(ws + m_hActiveWeapon);
        if (!h || h == 0xFFFFFFFF) return 0;
        uintptr_t es = SafeRead<uintptr_t>(client + dwEntityList);
        if (!es) return 0;
        uintptr_t chunk = SafeRead<uintptr_t>(es + 0x10 + 8 * ((h & 0x7FFF) >> 9));
        if (!chunk) return 0;
        return SafeRead<uintptr_t>(chunk + 0x70 * (h & 0x1FF));
    }

    static float GetBulletSpeed(uintptr_t wpn) {
        if (!wpn) return 800.f;
        char d[64]{};
        ReadDesigner(wpn, d, sizeof(d));
        if (strstr(d, "awp") || strstr(d, "ssg08")) return 3000.f;
        if (strstr(d, "scar20") || strstr(d, "g3sg1")) return 2200.f;
        if (strstr(d, "ak47") || strstr(d, "m4") ||
            strstr(d, "aug") || strstr(d, "sg556") ||
            strstr(d, "galilar") || strstr(d, "famas")) return 900.f;
        if (strstr(d, "deagle") || strstr(d, "revolver")) return 600.f;
        if (strstr(d, "glock") || strstr(d, "usp") ||
            strstr(d, "p250") || strstr(d, "tec9") ||
            strstr(d, "fiveseven") || strstr(d, "cz75a") ||
            strstr(d, "elite") || strstr(d, "hkp2000")) return 500.f;
        if (strstr(d, "mac10") || strstr(d, "mp9") ||
            strstr(d, "mp7") || strstr(d, "mp5") ||
            strstr(d, "ump45") || strstr(d, "p90") || strstr(d, "bizon")) return 700.f;
        return 800.f;
    }

    static std::vector<int> GetSelectedBones(const SilentAimConfig& s) {
        std::vector<int> bones;
        if (s.hit_head)    bones.push_back(7);
        if (s.hit_neck)    bones.push_back(6);
        if (s.hit_chest)   bones.push_back(23);
        if (s.hit_stomach) bones.push_back(3);
        if (s.hit_pelvis)  bones.push_back(1);
        if (s.hit_legs)    bones.push_back(17);
        if (bones.empty()) bones.push_back(7);
        return bones;
    }

    struct Candidate {
        Vec3  aim_angle;
        float fov;
        float dist;
        int   hp;
    };

    // ── Escaneo (sin cambios) ────────────────────────────────────────────
    static bool FindBestTarget(uintptr_t client, Vec3& out_angle, uintptr_t& out_pawn, Vec3& out_eye) {
        auto& s = g_cfg.rage.silent_aim;
        out_pawn = 0;

        uintptr_t local = SafeRead<uintptr_t>(client + dwLocalPlayerPawn);
        if (!local) return false;

        uintptr_t wpn = GetActiveWeapon(client, local);
        float bullet_speed = GetBulletSpeed(wpn);

        int  my_team = static_cast<int>(SafeRead<uint8_t>(local + m_iTeamNum));
        Vec3 origin = SafeRead<Vec3>(local + m_vOldOrigin);
        Vec3 eye = { origin.x, origin.y, origin.z + 64.f };
        out_eye = eye;

        uintptr_t es = SafeRead<uintptr_t>(client + dwEntityList);
        if (!es) return false;

        Vec3 cur = SafeRead<Vec3>(client + dwViewAngles);
        float fov_limit = (s.fov > 0.f) ? s.fov : 180.f;
        std::vector<int> bone_ids = GetSelectedBones(s);

        std::vector<Candidate> cands;
        cands.reserve(16);

        for (int i = 1; i < 64; ++i) {
            uintptr_t chunk = SafeRead<uintptr_t>(es + 0x10 + 8 * ((i & 0x7FFF) >> 9));
            if (!chunk) continue;
            uintptr_t controller = SafeRead<uintptr_t>(chunk + 0x70 * (i & 0x1FF));
            if (!controller) continue;

            uint32_t ph = SafeRead<uint32_t>(controller + m_hPlayerPawn);
            if (!ph || ph == 0xFFFFFFFF) continue;

            uintptr_t pchunk = SafeRead<uintptr_t>(es + 0x10 + 8 * ((ph & 0x7FFF) >> 9));
            if (!pchunk) continue;
            uintptr_t pawn = SafeRead<uintptr_t>(pchunk + 0x70 * (ph & 0x1FF));
            if (!pawn || pawn == local) continue;

            int hp = SafeRead<int>(pawn + m_iHealth);
            if (hp <= 0 || hp > 100) continue;

            int team = static_cast<int>(SafeRead<uint8_t>(pawn + m_iTeamNum));
            if (team != 2 && team != 3) continue;
            if (team == my_team && !s.target_team) continue;
            if (SafeRead<uint8_t>(pawn + m_lifeState) != 0) continue;

            uintptr_t node = SafeRead<uintptr_t>(pawn + m_pGameSceneNode);
            if (!node || SafeRead<bool>(node + m_bDormant)) continue;

            uintptr_t bone_arr = SafeRead<uintptr_t>(node + m_modelState + m_pBoneArray);
            if (!bone_arr) continue;

            Vec3 enemy_vel = SafeRead<Vec3>(pawn + m_vecAbsVelocity);

            for (int bi : bone_ids) {
                Vec3 bone = SafeRead<Vec3>(bone_arr + bi * 0x20);
                if (bone.x == 0.f && bone.y == 0.f && bone.z == 0.f) continue;
                if (bi == 7) bone.z += 6.f;

                float d = Dist(eye, bone);
                Vec3 target = bone;
                if (s.prediction) {
                    float t = (d / bullet_speed) * s.prediction_scale;
                    target.x += enemy_vel.x * t;
                    target.y += enemy_vel.y * t;
                    target.z += 0.5f * 800.f * t * t * 0.15f;
                }

                Vec3 ang = CalcAngle(eye, target);
                float dp = ang.x - cur.x;
                float dy = NormalizeAngle(ang.y - cur.y);
                float fov = sqrtf(dp * dp + dy * dy);
                if (fov > fov_limit) continue;

                cands.push_back({ ang, fov, d, hp });
                out_pawn = pawn;
                break;
            }
        }

        if (cands.empty()) return false;

        Candidate best = cands[0];
        switch (s.selection) {
        case 0: for (auto& c : cands) if (c.dist < best.dist) best = c; break;
        case 1: for (auto& c : cands) if (c.fov < best.fov)  best = c; break;
        case 2: for (auto& c : cands) if (c.hp < best.hp)   best = c; break;
        default: break;
        }

        out_angle = best.aim_angle;
        return true;
    }

    // ── CCSGOInput helpers (auto-fire edge) ──────────────────────────────
    static void ClearCsgoAttackInput() {
        void* pInput = Input::GetCSGOInput();
        if (!pInput) return;
        constexpr uint64_t kAttack = 1ull; // IN_ATTACK
        __try {
            auto* b = reinterpret_cast<uint8_t*>(pInput);
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn0) &= ~kAttack;
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn1) &= ~kAttack;
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn2) &= ~kAttack;
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn3) &= ~kAttack;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void EdgeCsgoAttackInput() {
        void* pInput = Input::GetCSGOInput();
        if (!pInput) return;
        constexpr uint64_t kAttack = 1ull;
        __try {
            auto* b = reinterpret_cast<uint8_t*>(pInput);
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn0) &= ~kAttack;
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn2) &= ~kAttack;
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn3) &= ~kAttack;
            *reinterpret_cast<uint64_t*>(b + kCsgoBtn1) |= kAttack;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // ── CreateMove hook ──────────────────────────────────────────────────
    using CreateMove_t = bool(__fastcall*)(void*, float, void*);
    static CreateMove_t oCreateMove = nullptr;
    static void* g_cmAddr = nullptr;
    static uint64_t g_hookHits = 0;
    static uint64_t g_scanHits = 0;

    // Estado de botones entre frames — solo para semis. Full-auto sostiene el bit.
    static bool s_semiNeedRelease = false;
    static bool s_autoHeld = false;

    static bool __fastcall hkCreateMove(void* thisptr, float sampleTime, void* cmdRaw) {
        bool r = oCreateMove(thisptr, sampleTime, cmdRaw);
        ++g_hookHits;

        if (!cmdRaw) return r;
        auto* cmd = reinterpret_cast<CUserCmd*>(cmdRaw);
        auto& s = g_cfg.rage.silent_aim;
        if (!s.enabled) return r;

        bool key_down = IsHotkeyDown(s.key);
        bool armed = (s.key == 0) ? true : key_down;
        if (!armed) return r;

        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return r;

        Vec3 ang{}; uintptr_t pawn = 0; Vec3 eye{};
        if (!FindBestTarget(client, ang, pawn, eye)) return r;
        ++g_scanHits;

        // ── FIX 1: view angle por PB + sub-tick ──────────────────────────
        QAngle_t qang{ ang.x, ang.y, 0.f };
        CBaseUserCmdPB* base = cmd->csgoUserCmd.pBaseCmd;
        if (base && base->pViewAngles
            && reinterpret_cast<uintptr_t>(base->pViewAngles) > 0x10000ull) {
            base->pViewAngles->angValue = qang;
            base->SetBits(BASE_BITS_VIEWANGLES);
        }
        cmd->SetSubTickAngle(qang);

        // ── FIX 2: auto-fire con edge correcto ───────────────────────────
        bool firing = (cmd->nButtons.nValue & IN_ATTACK) != 0;
        if (s.auto_fire) {
            firing = true;
            // Semi necesita release → press. Full-auto sostiene.
            // Sin ActiveWeaponIsSemi del archivo grande, usamos defindex simple:
            const bool semi = [&]() -> bool {
                uintptr_t wpn = GetActiveWeapon(client, SafeRead<uintptr_t>(client + dwLocalPlayerPawn));
                if (!wpn) return false;
                char d[64]{}; ReadDesigner(wpn, d, sizeof(d));
                return strstr(d, "awp") || strstr(d, "ssg08")
                    || strstr(d, "deagle") || strstr(d, "revolver")
                    || strstr(d, "glock") || strstr(d, "usp")
                    || strstr(d, "p250") || strstr(d, "tec9")
                    || strstr(d, "fiveseven") || strstr(d, "cz75a")
                    || strstr(d, "elite") || strstr(d, "hkp2000");
                }();

            if (semi) {
                // forzar release, luego edge
                cmd->nButtons.nValue &= ~IN_ATTACK;
                cmd->nButtons.nValueChanged &= ~IN_ATTACK;
                if (base && base->pInButtonState
                    && reinterpret_cast<uintptr_t>(base->pInButtonState) > 0x10000ull) {
                    base->pInButtonState->nValue &= ~IN_ATTACK;
                    base->pInButtonState->nValueChanged &= ~IN_ATTACK;
                }
                ClearCsgoAttackInput();
            }

            const bool alreadyDown = s_autoHeld || (cmd->nButtons.nValue & IN_ATTACK) != 0;
            cmd->nButtons.nValue |= IN_ATTACK;
            if (semi || !alreadyDown) cmd->nButtons.nValueChanged |= IN_ATTACK;

            if (base && base->pInButtonState
                && reinterpret_cast<uintptr_t>(base->pInButtonState) > 0x10000ull) {
                base->pInButtonState->nValue |= IN_ATTACK;
                if (semi || !alreadyDown) base->pInButtonState->nValueChanged |= IN_ATTACK;
                base->pInButtonState->SetBits(
                    BUTTON_STATE_PB_BITS_BUTTONSTATE1 |
                    BUTTON_STATE_PB_BITS_BUTTONSTATE2 |
                    BUTTON_STATE_PB_BITS_BUTTONSTATE3);
                base->SetBits(BASE_BITS_BUTTONPB);
            }

            if (semi) { s_semiNeedRelease = true; EdgeCsgoAttackInput(); }
            else { s_autoHeld = true; }
        }

        // ── FIX 3: bind attack history (lo que faltaba) ──────────────────
        if (firing) {
            const int count = cmd->csgoUserCmd.inputHistoryField.nCurrentSize;
            int idx = cmd->csgoUserCmd.nAttack1StartHistoryIndex;
            if (idx < 0 && count > 0) idx = count - 1;
            if (idx >= count)         idx = count > 0 ? count - 1 : -1;
            if (idx >= 0 && count > 0) {
                cmd->csgoUserCmd.SetAttack1StartHistoryIndex(idx);
                // narrow = true → escribe solo la entrada de historial,
                // no el base cmd. Eso es lo que lo hace SILENT.
                Vector_t eyeV{ eye.x, eye.y, eye.z };
                cmd->SetAttackHistoryFire(idx, qang, eyeV, /*narrow=*/true);
            }
        }

        // Cierre del ciclo semi: release en el siguiente CreateMove
        if (s_semiNeedRelease) {
            cmd->nButtons.nValue &= ~IN_ATTACK;
            cmd->nButtons.nValueChanged &= ~IN_ATTACK;
            if (base && base->pInButtonState
                && reinterpret_cast<uintptr_t>(base->pInButtonState) > 0x10000ull) {
                base->pInButtonState->nValue &= ~IN_ATTACK;
                base->pInButtonState->nValueChanged &= ~IN_ATTACK;
            }
            ClearCsgoAttackInput();
            s_semiNeedRelease = false;
            s_autoHeld = false;
        }

        return r;
    }

    // ── Pattern scan ─────────────────────────────────────────────────────
    static void* FindPattern(uintptr_t base, size_t size, const uint8_t* sig, const char* mask) {
        size_t mlen = strlen(mask);
        if (size < mlen) return nullptr;
        uint8_t* p = reinterpret_cast<uint8_t*>(base);
        for (size_t i = 0; i <= size - mlen; ++i) {
            bool ok = true;
            for (size_t j = 0; j < mlen; ++j) {
                if (mask[j] == 'x' && p[i + j] != sig[j]) { ok = false; break; }
            }
            if (ok) return &p[i];
        }
        return nullptr;
    }

    static bool InstallHook() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return false;

        const uint8_t sig[] = {
            0x48,0x8B,0xC4,0x4C,0x89,0x40,0x18,0x48,0x89,0x48,0x20,0x55,
            0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D
        };
        const char* mask = "xxxxxxxxxxxxxxxxxxxxxxxx";

        void* addr = FindPattern(client, 0x4000000, sig, mask);
        if (!addr) {
            OutputDebugStringA("[silent] CreateMove signature NOT found — re-derive\n");
            return false;
        }
        g_cmAddr = addr;

        if (MH_CreateHook(addr, &hkCreateMove, reinterpret_cast<void**>(&oCreateMove)) != MH_OK) {
            OutputDebugStringA("[silent] MH_CreateHook failed\n");
            return false;
        }
        if (MH_EnableHook(addr) != MH_OK) {
            OutputDebugStringA("[silent] MH_EnableHook failed\n");
            return false;
        }
        OutputDebugStringA("[silent] CreateMove hook installed\n");
        return true;
    }

    void Init() {
        if (MH_Initialize() != MH_OK)
            OutputDebugStringA("[silent] MH_Initialize failed\n");
        InstallHook();
        g_running.store(true, std::memory_order_release);
    }

    void Shutdown() {
        g_running.store(false, std::memory_order_release);
        if (g_cmAddr) {
            MH_DisableHook(g_cmAddr);
            MH_RemoveHook(g_cmAddr);
            g_cmAddr = nullptr;
        }
        MH_Uninitialize();
    }

    void RenderFovOverlay() {}
}