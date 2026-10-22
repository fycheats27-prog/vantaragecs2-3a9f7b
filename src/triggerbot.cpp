// --- triggerbot.cpp ---
#include "triggerbot.h"
#include "aimbot.h"
#include "config.h"
#include "memory.h"
#include "debug.h"
#include <Windows.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Features::Rage::Triggerbot {

    struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };

    static constexpr float kFallbackFov = 180.f;

    static constexpr uintptr_t dwEntityList = 0x271A908;
    static constexpr uintptr_t dwLocalPlayerPawn = 0x2565718;
    static constexpr uintptr_t dwViewAngles = 0x257BCC8;
    static constexpr uintptr_t dwForceAttack = 0x2233ED0;

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

    static constexpr int kAttackOn = 65537; // 0x10001
    static constexpr int kAttackOff = 256;   // 0x100

    static std::atomic<bool> g_running{ false };
    static std::thread g_thread;

    template<typename T>
    static T SafeRead(uintptr_t addr) {
        if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T{};
        __try { return *reinterpret_cast<T*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
    }

    static void SafeWrite(uintptr_t addr, int val) {
        if (!addr) return;
        __try { *reinterpret_cast<int*>(addr) = val; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
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

    static std::vector<int> GetSelectedBones(const TriggerWeaponConfig& w) {
        std::vector<int> bones;
        if (w.hit_head)    bones.push_back(7);
        if (w.hit_neck)    bones.push_back(6);
        if (w.hit_chest)   bones.push_back(23);
        if (w.hit_stomach) bones.push_back(3);
        if (w.hit_pelvis)  bones.push_back(1);
        if (w.hit_legs)    bones.push_back(17);
        if (bones.empty()) bones.push_back(7);
        return bones;
    }

    static float Dist(const Vec3& a, const Vec3& b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    static Vec3 ForwardFromAngles(const Vec3& ang) {
        float rx = ang.x * 0.01745329251f;
        float ry = ang.y * 0.01745329251f;
        return { cosf(rx) * cosf(ry), cosf(rx) * sinf(ry), -sinf(rx) };
    }

    static float AngleBetween(const Vec3& a, const Vec3& b) {
        float la = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
        float lb = sqrtf(b.x * b.x + b.y * b.y + b.z * b.z);
        if (la < 0.001f || lb < 0.001f) return 180.f;
        float dot = (a.x * b.x + a.y * b.y + a.z * b.z) / (la * lb);
        if (dot > 1.f)  dot = 1.f;
        if (dot < -1.f) dot = -1.f;
        return acosf(dot) * 57.29577951308232f;
    }

    static uintptr_t FindTargetOnCrosshair(
        uintptr_t client, uintptr_t local, uintptr_t es,
        const Vec3& eye, const Vec3& cur_view,
        int my_team, const TriggerWeaponConfig& w,
        bool target_team)
    {
        Vec3 view_dir = ForwardFromAngles(cur_view);
        uintptr_t best_pawn = 0;
        float fov_limit = (w.fov > 0.f) ? w.fov : kFallbackFov;
        float best_angle = fov_limit;
        std::vector<int> bone_ids = GetSelectedBones(w);

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

            uint8_t team_b = SafeRead<uint8_t>(pawn + m_iTeamNum);
            int team = team_b;
            if (team != 2 && team != 3) continue;
            if (team == my_team && !target_team) continue;

            if (SafeRead<uint8_t>(pawn + m_lifeState) != 0) continue;

            uintptr_t node = SafeRead<uintptr_t>(pawn + m_pGameSceneNode);
            if (!node || SafeRead<bool>(node + m_bDormant)) continue;

            uintptr_t ba = SafeRead<uintptr_t>(node + m_modelState + m_pBoneArray);
            if (!ba) continue;

            Vec3 vel = SafeRead<Vec3>(pawn + m_vecAbsVelocity);

            float pawn_min_angle = 180.f;
            for (int bone_idx : bone_ids) {
                Vec3 bone = SafeRead<Vec3>(ba + bone_idx * 0x20);
                if (bone.x == 0.f && bone.y == 0.f && bone.z == 0.f) continue;
                if (bone_idx == 7) bone.z += 6.f;

                Vec3 target = bone;
                if (w.prediction) {
                    float d = Dist(eye, bone);
                    float t = d / 800.f;
                    target.x += vel.x * t;
                    target.y += vel.y * t;
                }

                Vec3 dir = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
                float ang = AngleBetween(view_dir, dir);
                if (ang < pawn_min_angle) pawn_min_angle = ang;
            }

            if (pawn_min_angle < best_angle) {
                best_angle = pawn_min_angle;
                best_pawn = pawn;
            }
        }
        return best_pawn;
    }

    static void Worker() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        static uintptr_t last_target = 0;
        static uint64_t  first_seen_ms = 0;
        static bool      attack_virtual = false;
        static bool      toggle_active = false;
        static bool      toggle_prev_key = false;

        while (g_running.load(std::memory_order_acquire)) {
            auto& t = g_cfg.rage.trigger;

            // ── Hotkey ──
            bool key_down = IsHotkeyDown(t.key);
            bool hotkey_active = false;

            if (t.key == 0) {
                hotkey_active = true;
            }
            else if (t.toggle_mode) {
                if (key_down && !toggle_prev_key) toggle_active = !toggle_active;
                toggle_prev_key = key_down;
                hotkey_active = toggle_active;
            }
            else {
                hotkey_active = key_down;
            }

            if (!t.enabled || !hotkey_active) {
                if (attack_virtual) { SafeWrite(client + dwForceAttack, kAttackOff); attack_virtual = false; }
                last_target = 0;
                first_seen_ms = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            // ── Categoría del arma actual ──
            WeaponCategory cat = Features::Rage::Aimbot::GetCurrentCategory();
            const TriggerWeaponConfig& w = t.weapons[(int)cat];

            if (!w.enabled) {
                if (attack_virtual) { SafeWrite(client + dwForceAttack, kAttackOff); attack_virtual = false; }
                last_target = 0;
                first_seen_ms = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            uintptr_t local = SafeRead<uintptr_t>(client + dwLocalPlayerPawn);
            if (!local) {
                if (attack_virtual) { SafeWrite(client + dwForceAttack, kAttackOff); attack_virtual = false; }
                last_target = 0;
                first_seen_ms = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            uintptr_t es = SafeRead<uintptr_t>(client + dwEntityList);
            if (!es) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            uint8_t my_team_b = SafeRead<uint8_t>(local + m_iTeamNum);
            int  my_team = my_team_b;
            Vec3 local_origin = SafeRead<Vec3>(local + m_vOldOrigin);
            Vec3 eye = local_origin;
            eye.z += 64.f;
            Vec3 cur_view = SafeRead<Vec3>(client + dwViewAngles);

            // ── Detección ──
            uintptr_t target = FindTargetOnCrosshair(
                client, local, es, eye, cur_view, my_team, w, t.target_team);

            // ── Si el usuario ya está disparando, no interferimos ──
            bool user_lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (user_lmb) {
                if (attack_virtual) { SafeWrite(client + dwForceAttack, kAttackOff); attack_virtual = false; }
                last_target = 0;
                first_seen_ms = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            // ── Delay logic ──
            uint64_t now_ms = GetTickCount64();

            if (!target) {
                if (attack_virtual) { SafeWrite(client + dwForceAttack, kAttackOff); attack_virtual = false; }
                last_target = 0;
                first_seen_ms = 0;
            }
            else if (target != last_target) {
                last_target = target;
                first_seen_ms = now_ms;
                if (attack_virtual) { SafeWrite(client + dwForceAttack, kAttackOff); attack_virtual = false; }
            }
            else {
                if (now_ms - first_seen_ms >= (uint64_t)w.delay_ms) {
                    if (!attack_virtual) {
                        SafeWrite(client + dwForceAttack, kAttackOn);
                        attack_virtual = true;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    void Init() {
        g_running.store(true, std::memory_order_release);
        g_thread = std::thread(Worker);
    }

    void Shutdown() {
        g_running.store(false, std::memory_order_release);
        if (g_thread.joinable()) g_thread.join();

        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (client) SafeWrite(client + dwForceAttack, kAttackOff);
    }
}