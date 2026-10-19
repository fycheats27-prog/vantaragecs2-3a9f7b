// language: C++, file: src/bhop.cpp
// *Bhop por dwForceJump + autostrafe por CUserCmd con offsets reales.*

#include "bhop.h"
#include "config.h"
#include "sdk.h"
#include "entity.h"
#include "memory.h"
#include "debug.h"
#include <Windows.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace Features::Misc::Bhop {

    // ── RVAs ──
    static constexpr uintptr_t kForceJumpRVA = 0x22343E0;
    static constexpr int kJumpOn = 65537;
    static constexpr int kJumpOff = 256;

    // ── schema C_BaseEntity ──
    static constexpr int k_m_iHealth = 0x34C;
    static constexpr int k_m_iTeamNum = 0x3E7;
    static constexpr int k_m_lifeState = 0x354;
    static constexpr int k_m_fFlags = 0x3F4;
    static constexpr int k_m_vecAbsVelocity = 0x3F8;

    static constexpr int kFL_ONGROUND = 1 << 0;

    // ── schema CUserCmd ──
    static constexpr int k_m_csgoUserCmd_base = 0x40;   // CUserCmd -> CCSGOUserCmdPB*

    // ── schema CCSGOUserCmdPB ──
    static constexpr int k_pBaseCmd_offset = 0x00;      // CCSGOUserCmdPB -> CBaseUserCmdPB*

    // ── schema CBaseUserCmdPB ──
    static constexpr int k_buttons_pb = 0x38;
    static constexpr int k_viewangles_pb = 0x208;  // LAYOUT: inline CMsgQAngle (CS2)
    static constexpr int k_forwardmove = 0x58;    // float
    static constexpr int k_leftmove = 0x5C;    // float (sidemove)
    static constexpr int k_upmove = 0x60;    // float

    // ── schema CInButtonStatePB ──
    static constexpr int k_buttonstate1 = 0x18;

    // ── schema CMsgQAngle ──
    static constexpr int k_qa_pitch = 0x00;
    static constexpr int k_qa_yaw = 0x04;
    static constexpr int k_qa_roll = 0x08;

    static constexpr int kIN_JUMP = 1 << 1;

    static constexpr float kPI = 3.14159265358979f;
    static inline float Rad2Deg(float r) { return r * (180.0f / kPI); }

    static std::atomic<bool> g_running{ false };
    static std::thread       g_thread;

    static inline float NormalizeYaw(float a) {
        while (a > 180.0f) a -= 360.0f;
        while (a < -180.0f) a += 360.0f;
        return a;
    }

    // ═══════════════════════════════════════════════════════════════
    //  HOOK ENTRY — autostrafe
    // ═══════════════════════════════════════════════════════════════

    void OnCreateMove(void* pCmd) {
        if (!pCmd) return;
        if (!g_cfg.misc.movement.auto_strafe) return;

        // wrapper → CCSGOUserCmdPB → CBaseUserCmdPB
        uintptr_t csgo_cmd = *reinterpret_cast<uintptr_t*>((uintptr_t)pCmd + k_m_csgoUserCmd_base);
        if (!csgo_cmd) return;
        uintptr_t base_pb = *reinterpret_cast<uintptr_t*>(csgo_cmd + k_pBaseCmd_offset);
        if (!base_pb) return;

        uintptr_t pawn = SDK::GetLocalPlayerPawn();
        if (!pawn) return;

        int      hp = Memory::Read<int>(pawn + k_m_iHealth);
        uint8_t  life = Memory::Read<uint8_t>(pawn + k_m_lifeState);
        uint32_t flags = Memory::Read<uint32_t>(pawn + k_m_fFlags);
        if (hp <= 0 || life != 0) return;
        if (flags & kFL_ONGROUND) return;

        Vec3 vel = Memory::Read<Vec3>(pawn + k_m_vecAbsVelocity);
        float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
        if (speed < 5.0f) return;

        // LAYOUT: inline angle block. Si tu dump dice puntero, cambia a:
        //   uintptr_t pViewAngles = *reinterpret_cast<uintptr_t*>(base_pb + k_viewangles_pb);
        //   if (!pViewAngles) return;
        uintptr_t pViewAngles = base_pb + k_viewangles_pb;

        float cur_yaw = Memory::Read<float>(pViewAngles + k_qa_yaw);
        float vel_yaw = Rad2Deg(atan2f(vel.y, vel.x));

        // ángulo óptimo real: atan(30 / speed) — decrece con velocidad
        float ideal = Rad2Deg(atanf(30.0f / speed));
        if (ideal < 1.0f)  ideal = 1.0f;
        if (ideal > 60.0f) ideal = 60.0f;

        // lado por signo de (cur_yaw - vel_yaw) — se mantiene, no oscila
        float diff = NormalizeYaw(cur_yaw - vel_yaw);
        bool  strafe_left = (diff > 0.0f);
        float target_yaw = NormalizeYaw(vel_yaw + (strafe_left ? -ideal : ideal));

        float delta = NormalizeYaw(target_yaw - cur_yaw);
        float step = delta * 0.9f;
        if (step > 45.0f) step = 45.0f;
        if (step < -45.0f) step = -45.0f;

        Memory::Write<float>(pViewAngles + k_qa_yaw, NormalizeYaw(cur_yaw + step));

        float side = strafe_left ? -1.0f : 1.0f;
        Memory::Write<float>(base_pb + k_leftmove, side * 450.0f);
        Memory::Write<float>(base_pb + k_forwardmove, 0.0f);
        Memory::Write<float>(base_pb + k_upmove, 0.0f);

        Debug::g_bhop.flags = (uint32_t)speed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  THREAD — bhop por dwForceJump
    // ═══════════════════════════════════════════════════════════════

    static void Worker() {
        while (g_running.load(std::memory_order_acquire)) {
            uintptr_t client = Memory::GetModuleBase("client.dll");
            if (!client) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            bool bhop_on = g_cfg.misc.movement.bhop;

            Debug::g_bhop.hook_alive = true;
            Debug::g_bhop.bhop_enabled = bhop_on;
            Debug::g_bhop.tick++;

            uintptr_t pawn = SDK::GetLocalPlayerPawn();
            Debug::g_bhop.pawn = pawn;

            if (pawn) {
                Debug::g_bhop.hp = Memory::Read<int>(pawn + k_m_iHealth);
                Debug::g_bhop.team = (int)Memory::Read<uint8_t>(pawn + k_m_iTeamNum);
                Debug::g_bhop.life = (int)Memory::Read<uint8_t>(pawn + k_m_lifeState);
                uint32_t flags = Memory::Read<uint32_t>(pawn + k_m_fFlags);
                Debug::g_bhop.flags = flags;
                Debug::g_bhop.on_ground = (flags & kFL_ONGROUND) != 0;
            }
            Debug::g_bhop.jump_held = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

            if (bhop_on && pawn) {
                int      hp = Memory::Read<int>(pawn + k_m_iHealth);
                uint8_t  life = Memory::Read<uint8_t>(pawn + k_m_lifeState);
                uint32_t flags = Memory::Read<uint32_t>(pawn + k_m_fFlags);
                bool     on_ground = (flags & kFL_ONGROUND) != 0;
                bool     alive = (hp > 0 && life == 0);

                if (Debug::g_bhop.jump_held && on_ground && alive) {
                    Memory::Write<int>(client + kForceJumpRVA, kJumpOn);
                }
                else {
                    Memory::Write<int>(client + kForceJumpRVA, kJumpOff);
                }
            }

            Debug::Flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    void Init() {
        g_running.store(true, std::memory_order_release);
        g_thread = std::thread(Worker);
    }

    void Shutdown() {
        g_running.store(false, std::memory_order_release);
        if (g_thread.joinable())
            g_thread.join();
    }

} // namespace Features::Misc::Bhop