// --- aimbot.cpp ---
#include "aimbot.h"
#include "config.h"
#include "memory.h"
#include "debug.h"
#include <Windows.h>
#include <cmath>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace Features::Rage::Aimbot {

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

    static std::atomic<bool> g_running{ false };
    static std::thread g_thread;

    template<typename T>
    static T SafeRead(uintptr_t addr) {
        if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T{};
        __try { return *reinterpret_cast<T*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
    }

    static void SafeWriteFloat(uintptr_t addr, float val) {
        if (!addr) return;
        __try { *reinterpret_cast<float*>(addr) = val; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static float NormalizeAngle(float a) {
        while (a > 180.f) a -= 360.f;
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

    static WeaponCategory CategorizeWeapon(const char* designer) {
        if (!designer || !designer[0]) return WeaponCategory::Other;
        const char* d = designer;
        if (strncmp(d, "weapon_", 7) == 0) d += 7;

        if (strstr(d, "awp") || strstr(d, "ssg08") ||
            strstr(d, "scar20") || strstr(d, "g3sg1")) return WeaponCategory::Sniper;
        if (strstr(d, "m249") || strstr(d, "negev")) return WeaponCategory::MG;
        if (strstr(d, "nova") || strstr(d, "xm1014") ||
            strstr(d, "mag7") || strstr(d, "sawedoff")) return WeaponCategory::Shotgun;
        if (strstr(d, "mac10") || strstr(d, "mp9") || strstr(d, "mp7") ||
            strstr(d, "mp5sd") || strstr(d, "ump45") || strstr(d, "p90") ||
            strstr(d, "bizon")) return WeaponCategory::SMG;
        if (strstr(d, "glock") || strstr(d, "usp") || strstr(d, "hkp2000") ||
            strstr(d, "p250") || strstr(d, "deagle") || strstr(d, "revolver") ||
            strstr(d, "fiveseven") || strstr(d, "tec9") || strstr(d, "cz75a") ||
            strstr(d, "elite")) return WeaponCategory::Pistol;
        if (strstr(d, "ak47") || strstr(d, "m4a1") || strstr(d, "aug") ||
            strstr(d, "sg556") || strstr(d, "galilar") || strstr(d, "famas"))
            return WeaponCategory::Rifle;

        return WeaponCategory::Other;
    }

    static const char* CategoryName(WeaponCategory cat) {
        static const char* names[] = {
            "pistol", "rifle", "sniper", "smg", "shotgun", "mg", "other"
        };
        int idx = (int)cat;
        if (idx < 0 || idx >= (int)WeaponCategory::COUNT) return "other";
        return names[idx];
    }

    static std::vector<int> GetSelectedBones(const WeaponConfig& w) {
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

    int GetFov() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return 15;
        uintptr_t local = SafeRead<uintptr_t>(client + dwLocalPlayerPawn);
        if (!local) return 15;
        uintptr_t wpn = GetActiveWeapon(client, local);
        if (!wpn) return 15;
        char designer[64]{};
        ReadDesigner(wpn, designer, sizeof(designer));
        WeaponCategory cat = CategorizeWeapon(designer);
        return g_cfg.rage.aimbot.weapons[(int)cat].fov;
    }

    WeaponCategory GetCurrentCategory() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return WeaponCategory::Other;
        uintptr_t local = SafeRead<uintptr_t>(client + dwLocalPlayerPawn);
        if (!local) return WeaponCategory::Other;
        uintptr_t wpn = GetActiveWeapon(client, local);
        if (!wpn) return WeaponCategory::Other;
        char designer[64]{};
        ReadDesigner(wpn, designer, sizeof(designer));
        return CategorizeWeapon(designer);
    }

    const char* GetCurrentWeaponName() {
        static char s_name[64] = "none";
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return s_name;
        uintptr_t local = SafeRead<uintptr_t>(client + dwLocalPlayerPawn);
        if (!local) return s_name;
        uintptr_t wpn = GetActiveWeapon(client, local);
        if (!wpn) return s_name;
        char designer[64]{};
        ReadDesigner(wpn, designer, sizeof(designer));
        const char* p = designer;
        if (strncmp(p, "weapon_", 7) == 0) p += 7;
        snprintf(s_name, sizeof(s_name), "%s", p[0] ? p : "none");
        return s_name;
    }

    const char* GetCurrentCategoryName() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return "other";
        uintptr_t local = SafeRead<uintptr_t>(client + dwLocalPlayerPawn);
        if (!local) return "other";
        uintptr_t wpn = GetActiveWeapon(client, local);
        if (!wpn) return "other";
        char designer[64]{};
        ReadDesigner(wpn, designer, sizeof(designer));
        return CategoryName(CategorizeWeapon(designer));
    }

    struct Candidate {
        uintptr_t pawn;
        Vec3      aim_angle;
        float     fov;
        float     dist;
        int       bone_idx;
    };

    static void Worker() {
        uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        static uintptr_t cached_weapon = 0;
        static float     cached_bullet = 800.f;
        static WeaponCategory cached_cat = WeaponCategory::Rifle;
        static uint64_t  target_acquire_time = 0;
        static uintptr_t last_target_pawn = 0;

        static bool toggle_active = false;
        static bool toggle_prev_key = false;

        while (g_running.load(std::memory_order_acquire)) {
            auto& cfg = g_cfg.rage.aimbot;

            // ── Hotkey ──
            bool key_down = IsHotkeyDown(cfg.key);
            bool hotkey_active = false;

            if (cfg.key == 0) {
                hotkey_active = true;
            }
            else if (cfg.toggle_mode) {
                if (key_down && !toggle_prev_key) toggle_active = !toggle_active;
                toggle_prev_key = key_down;
                hotkey_active = toggle_active;
            }
            else {
                hotkey_active = key_down;
            }

            if (!cfg.enabled || !hotkey_active) {
                Debug::g_aim.active = 0;
                target_acquire_time = 0;
                last_target_pawn = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            Debug::g_aim.active = 1;

            uintptr_t local = SafeRead<uintptr_t>(client + dwLocalPlayerPawn);
            if (!local) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }

            // ── Arma actual ──
            uintptr_t wpn = GetActiveWeapon(client, local);
            if (wpn != cached_weapon) {
                cached_weapon = wpn;
                cached_bullet = GetBulletSpeed(wpn);
                char designer[64]{};
                ReadDesigner(wpn, designer, sizeof(designer));
                cached_cat = CategorizeWeapon(designer);
            }

            const WeaponConfig& w = cfg.weapons[(int)cached_cat];
            Debug::g_aim.fov = (w.fov > 0) ? w.fov : 180;

            int  my_team = SafeRead<uint8_t>(local + m_iTeamNum);
            Vec3 local_origin = SafeRead<Vec3>(local + m_vOldOrigin);
            Vec3 eye = local_origin;
            eye.z += 64.f;

            uintptr_t es = SafeRead<uintptr_t>(client + dwEntityList);
            if (!es) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }

            Vec3 cur = SafeRead<Vec3>(client + dwViewAngles);
            Debug::g_aim.cur_pitch = cur.x;
            Debug::g_aim.cur_yaw = cur.y;

            float fov_limit = (w.fov > 0) ? (float)w.fov : 180.f;
            std::vector<int> bone_ids = GetSelectedBones(w);

            std::vector<Candidate> cands;
            cands.reserve(16);

            // ── Escaneo ──
            for (int i = 1; i < 64; ++i) {
                uintptr_t chunk = SafeRead<uintptr_t>(es + 0x10 + 8 * ((i & 0x7FFF) >> 9));
                if (!chunk) continue;
                uintptr_t controller = SafeRead<uintptr_t>(chunk + 0x70 * (i & 0x1FF));
                if (!controller) continue;

                uint32_t pawn_handle = SafeRead<uint32_t>(controller + m_hPlayerPawn);
                if (!pawn_handle || pawn_handle == 0xFFFFFFFF) continue;

                uintptr_t pchunk = SafeRead<uintptr_t>(es + 0x10 + 8 * ((pawn_handle & 0x7FFF) >> 9));
                if (!pchunk) continue;
                uintptr_t pawn = SafeRead<uintptr_t>(pchunk + 0x70 * (pawn_handle & 0x1FF));
                if (!pawn || pawn == local) continue;

                int hp = SafeRead<int>(pawn + m_iHealth);
                if (hp <= 0 || hp > 100) continue;

                int team = SafeRead<uint8_t>(pawn + m_iTeamNum);
                if (team != 2 && team != 3) continue;
                if (team == my_team && !cfg.target_team) continue;

                if (SafeRead<uint8_t>(pawn + m_lifeState) != 0) continue;

                uintptr_t node = SafeRead<uintptr_t>(pawn + m_pGameSceneNode);
                if (!node || SafeRead<bool>(node + m_bDormant)) continue;

                uintptr_t bone_arr = SafeRead<uintptr_t>(node + m_modelState + m_pBoneArray);
                if (!bone_arr) continue;

                Vec3 enemy_vel = SafeRead<Vec3>(pawn + m_vecAbsVelocity);

                for (int bone_idx : bone_ids) {
                    Vec3 bone = SafeRead<Vec3>(bone_arr + bone_idx * 0x20);
                    if (bone.x == 0.f && bone.y == 0.f && bone.z == 0.f) continue;
                    if (bone_idx == 7) bone.z += 6.f;

                    float d = Dist(eye, bone);
                    Vec3 target = bone;
                    if (w.prediction) {
                        float t = (d / cached_bullet) * w.prediction_scale;
                        target.x += enemy_vel.x * t;
                        target.y += enemy_vel.y * t;
                        target.z += 0.5f * 800.f * t * t * 0.15f;
                    }

                    Vec3 aim_ang = CalcAngle(eye, target);
                    float dp = aim_ang.x - cur.x;
                    float dy = NormalizeAngle(aim_ang.y - cur.y);
                    float fov = sqrtf(dp * dp + dy * dy);

                    if (fov > fov_limit) continue;

                    cands.push_back({ pawn, aim_ang, fov, d, bone_idx });
                    break;
                }
            }

            if (cands.empty()) {
                Debug::g_aim.target_idx = -1;
                target_acquire_time = 0;
                last_target_pawn = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            int limit = w.max_targets;
            if (limit < 1)  limit = 1;
            if (limit > 12) limit = 12;
            if ((int)cands.size() > limit) {
                std::sort(cands.begin(), cands.end(),
                    [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });
                cands.resize(limit);
            }

            Candidate best = cands[0];
            for (auto& c : cands) if (c.fov < best.fov) best = c;

            uint64_t now_ms = GetTickCount64();
            if (best.pawn != last_target_pawn) {
                target_acquire_time = now_ms;
                last_target_pawn = best.pawn;
            }

            if (now_ms - target_acquire_time < (uint64_t)w.reaction_ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            // ── Aplicar aim ──
            Debug::g_aim.target_idx = 1;
            Debug::g_aim.target_dist = best.dist;
            Debug::g_aim.aim_pitch = best.aim_angle.x;
            Debug::g_aim.aim_yaw = best.aim_angle.y;
            Debug::g_aim.bone = best.bone_idx;

            float s = w.smooth;
            if (s < 0.f) s = 0.f;
            if (s > 1.f) s = 1.f;
            float factor = 1.f - s * 0.95f;

            float new_pitch = cur.x + (best.aim_angle.x - cur.x) * factor;
            float new_yaw = NormalizeAngle(cur.y + NormalizeAngle(best.aim_angle.y - cur.y) * factor);

            SafeWriteFloat(client + dwViewAngles + 0, new_pitch);
            SafeWriteFloat(client + dwViewAngles + 4, new_yaw);

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
    }
}