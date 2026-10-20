// --- aimbot.cpp ---
#include "aimbot.h"
#include "config.h"
#include "memory.h"
#include "debug.h"
#include <Windows.h>
#include <cmath>
#include <cstdint>

namespace Features::Rage::Aimbot {

    struct Vec3 { float x, y, z; };
    static constexpr int B_HEAD = 7;

    static inline float norm_yaw(float y) {
        while (y > 180.f) y -= 360.f;
        while (y < -180.f) y += 360.f;
        return y;
    }

    static Vec3 calc_angle(const Vec3& src, const Vec3& dst) {
        Vec3 d = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
        float hyp = sqrtf(d.x * d.x + d.y * d.y);
        Vec3 a;
        a.x = atan2f(-d.z, hyp) * 57.29577951308232f;
        a.y = atan2f(d.y, d.x) * 57.29577951308232f;
        a.z = 0.f;
        return a;
    }

    static float angle_dist(float pa, float ya, float pb, float yb) {
        float dp = pa - pb;
        float dy = norm_yaw(ya - yb);
        return sqrtf(dp * dp + dy * dy);
    }

    static Vec3 get_eye(uintptr_t pawn) {
        uintptr_t node = Memory::Read<uintptr_t>(pawn + 0x330);
        if (!node) return { 0, 0, 0 };
        Vec3 feet = Memory::Read<Vec3>(node + 0xC8);
        feet.z += 64.f;
        return feet;
    }

    static Vec3 get_bone(uintptr_t pawn, int idx) {
        uintptr_t node = Memory::Read<uintptr_t>(pawn + 0x330);
        if (!node) return { 0, 0, 0 };
        uintptr_t bones = Memory::Read<uintptr_t>(node + 0x140 + 0x80);
        if (!bones) return { 0, 0, 0 };
        return Memory::Read<Vec3>(bones + (uintptr_t)idx * 0x20);
    }

    void OnCreateMove(void* pCmd) {
        auto& cfg = g_cfg.rage.aimbot;

        // Forzamos el aimbot para probar (Ignoramos el menú temporalmente)
        cfg.enabled = true;
        cfg.key = 0;
        cfg.fov = 180;

        Debug::g_aim.active = 1;

        if (!pCmd) return;

        uintptr_t g_client = Memory::GetModuleBase("client.dll");
        if (!g_client) return;

        // Obtener ángulos del CUserCmd (Mismo método que tu bhop)
        uintptr_t csgo_cmd = *reinterpret_cast<uintptr_t*>((uintptr_t)pCmd + 0x40);
        if (!csgo_cmd) return;
        uintptr_t base_pb = *reinterpret_cast<uintptr_t*>(csgo_cmd + 0x00);
        if (!base_pb) return;
        uintptr_t pViewAngles = base_pb + 0x208;

        float cur_pitch = Memory::Read<float>(pViewAngles + 0);
        float cur_yaw = Memory::Read<float>(pViewAngles + 4);

        Debug::g_aim.cur_pitch = cur_pitch;
        Debug::g_aim.cur_yaw = cur_yaw;

        uintptr_t local = Memory::Read<uintptr_t>(g_client + 0x2565718);
        if (!local) return;
        Vec3 eye = get_eye(local);
        int my_team = Memory::Read<uint8_t>(local + 0x3E7);

        uintptr_t entity_list = Memory::Read<uintptr_t>(g_client + 0x271A908);
        if (!entity_list) return;

        uintptr_t best_pawn = 0;
        Vec3 best_bone = {};
        float best_score = -1e9f;

        for (int i = 1; i <= 64; ++i) {
            uintptr_t chunk = Memory::Read<uintptr_t>(entity_list + 0x10 + 8 * ((i & 0x7FFF) >> 9));
            if (!chunk) continue;

            uintptr_t controller = Memory::Read<uintptr_t>(chunk + 0x70 * (i & 0x1FF));
            if (!controller) continue;

            uint32_t pawn_handle = Memory::Read<uint32_t>(controller + 0x92C);
            if (!pawn_handle || pawn_handle == 0xFFFFFFFF) continue;

            uintptr_t pchunk = Memory::Read<uintptr_t>(entity_list + 0x10 + 8 * ((pawn_handle & 0x7FFF) >> 9));
            if (!pchunk) continue;

            uintptr_t p = Memory::Read<uintptr_t>(pchunk + 0x70 * (pawn_handle & 0x1FF));
            if (!p || p == local) continue;

            int hp = Memory::Read<int>(p + 0x34C);
            if (hp <= 0 || hp > 100) continue;

            int team = Memory::Read<uint8_t>(p + 0x3E7);
            if (team == my_team) continue;

            Vec3 bp = get_bone(p, B_HEAD);
            if (bp.x == 0.f && bp.y == 0.f && bp.z == 0.f) continue;

            Vec3 want = calc_angle(eye, bp);
            float ang = angle_dist(cur_pitch, cur_yaw, want.x, want.y);

            if (ang < 180.f) { // FOV 180
                float d = sqrtf((bp.x - eye.x) * (bp.x - eye.x) + (bp.y - eye.y) * (bp.y - eye.y) + (bp.z - eye.z) * (bp.z - eye.z));
                float s = -ang - d / 600.f;
                if (s > best_score) {
                    best_score = s;
                    best_pawn = p;
                    best_bone = bp;
                }
            }
        }

        if (!best_pawn) return;

        Vec3 target_ang = calc_angle(eye, best_bone);
        Debug::g_aim.target_idx = (int)best_pawn;
        Debug::g_aim.aim_pitch = target_ang.x;
        Debug::g_aim.aim_yaw = target_ang.y;

        // Suavizado (0.4 para que no sea instantáneo, 1.0 para que lo sea)
        float smooth = 0.40f;
        float new_pitch = cur_pitch + (target_ang.x - cur_pitch) * smooth;
        float new_yaw = norm_yaw(cur_yaw + norm_yaw(target_ang.y - cur_yaw) * smooth);

        // Escribimos directamente en el CUserCmd
        Memory::Write<float>(pViewAngles + 0, new_pitch);
        Memory::Write<float>(pViewAngles + 4, new_yaw);
    }
}