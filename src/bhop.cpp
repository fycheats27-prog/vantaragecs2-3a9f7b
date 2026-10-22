// language: C++, file: src/bhop_advanced.cpp
// Advanced Bhop & Autostrafe with Subtick support (if available) and fallbacks.

#include "bhop.h"
#include "config.h"
#include "sdk.h"
#include "entity.h"
#include "memory.h"
#include "debug.h"
#include <Windows.h>
#include <cmath>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <chrono>

namespace Features::Misc::Bhop {

    // ── CS2 Structures (Simplified for integration) ──────────────────────────
    struct CBaseUserCmdPB {
        uint32_t command_number;
        uint32_t tick_count;
        float viewangles_x;
        float viewangles_y;
        float viewangles_z;
        uint32_t buttons;
        uint8_t pad_0x44[0x14];
        float forwardmove;
        float leftmove;
        float upmove;
        // Add other fields if needed for your hook
    };

    // ── Offsets ───────────────────────────────────────────────────────────────
    static constexpr uintptr_t kForceJumpRVA = 0x22343E0;
    static constexpr int       kJumpOn = 65537;
    static constexpr int       kJumpOff = 256;

    static constexpr int k_m_iHealth = 0x34C;
    static constexpr int k_m_lifeState = 0x354;
    static constexpr int k_m_fFlags = 0x3F4;
    static constexpr int k_m_vecAbsVelocity = 0x3F8;
    static constexpr int k_m_vecVelocity = 0x430; // Networked velocity
    static constexpr int k_m_pMovementServices = 0x1330;
    static constexpr int kFL_ONGROUND = 1 << 0;

    // ── Physics constants ─────────────────────────────────────────────────────
    static constexpr float kPI = 3.14159265358979f;
    static constexpr float kAirAccelerate = 12.f;
    static constexpr float kWishSpeedCap = 30.f;
    static constexpr float kAirAccelProduct = kAirAccelerate * kWishSpeedCap;
    static constexpr float kUnitsScale = 1.f;
    static constexpr float kMinStrafeSpeed = 8.f;
    static constexpr float kFastStopMinSpeed = 15.f;

    static inline float Rad2Deg(float r) { return r * (180.f / kPI); }
    static inline float Deg2Rad(float d) { return d * (kPI / 180.f); }
    static inline float NormalizeYaw(float a) {
        while (a > 180.f) a -= 360.f;
        while (a < -180.f) a += 360.f;
        return a;
    }

    // ── Per-tick state ────────────────────────────────────────────────────────
    static float s_last_fwd = 0.f;
    static float s_last_side = 0.f;
    static bool  s_was_moving = false;
    static int   s_stop_ticks = 0;
    static bool  s_was_on_ground = false;
    static int   s_landing_ticks = 0;
    static constexpr int kLandingWindow = 2;

    // ── Worker thread state ───────────────────────────────────────────────────
    static std::atomic<bool>   g_running{ false };
    static std::atomic<bool>   g_hook_alive{ false };
    static std::thread             g_thread;
    static std::mutex              g_mtx;
    static std::condition_variable g_cv;

    static LONG FilterSEH(DWORD code) {
        return (code == EXCEPTION_ACCESS_VIOLATION)
            ? EXCEPTION_EXECUTE_HANDLER
            : EXCEPTION_CONTINUE_SEARCH;
    }

    static inline bool PawnAlive(uintptr_t pawn) {
        if (pawn < 0x10000) return false;
        int     hp = *reinterpret_cast<int*>(pawn + k_m_iHealth);
        uint8_t life = *reinterpret_cast<uint8_t*>(pawn + k_m_lifeState);
        return (hp > 0 && life == 0);
    }

    struct Vel2D { float x, y, speed; };
    static Vel2D ReadVelocity2D(uintptr_t pawn) {
        struct V3 { float x, y, z; };
        // Try networked velocity first, then abs
        V3 v = *reinterpret_cast<V3*>(pawn + k_m_vecVelocity);
        if (std::abs(v.x) < 0.1f && std::abs(v.y) < 0.1f) {
            v = *reinterpret_cast<V3*>(pawn + k_m_vecAbsVelocity);
        }
        float spd = std::sqrt(v.x * v.x + v.y * v.y);
        return { v.x, v.y, spd };
    }

    static void ComputeStrafeWishdir(float vel_x, float vel_y, float speed,
        float cur_yaw_deg,
        float& out_fwd, float& out_side)
    {
        float vel_yaw = Rad2Deg(std::atan2(vel_y, vel_x));
        float diff = NormalizeYaw(vel_yaw - cur_yaw_deg);
        bool  accel_ccw = diff > 0.f;

        float cos_ideal = std::clamp(kAirAccelProduct / (2.f * speed), 0.f, 1.f);
        float ideal_deg = Rad2Deg(std::acos(cos_ideal));
        ideal_deg = std::clamp(ideal_deg, 0.5f, 89.f);

        float wishdir_yaw = NormalizeYaw(vel_yaw + (accel_ccw ? ideal_deg : -ideal_deg));
        float offset = NormalizeYaw(wishdir_yaw - cur_yaw_deg);
        float rad = Deg2Rad(offset);

        out_fwd = std::cos(rad) * kUnitsScale;
        out_side = std::sin(rad) * kUnitsScale;
    }

    void OnCreateMove(void* pCmd) {
        if (!pCmd) return;

        uintptr_t pawn = SDK::GetLocalPlayerPawn();
        if (!pawn) return;

        __try {
            if (!PawnAlive(pawn)) return;

            CBaseUserCmdPB* cmd = reinterpret_cast<CBaseUserCmdPB*>(pCmd);
            if ((uintptr_t)cmd < 0x10000) return;

            uint32_t flags = *reinterpret_cast<uint32_t*>(pawn + k_m_fFlags);
            bool     on_ground = (flags & kFL_ONGROUND) != 0;
            bool     space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

            float* fwd = &cmd->forwardmove;
            float* side = &cmd->leftmove;
            float* up = &cmd->upmove;

            float orig_fwd = *fwd;
            float orig_side = *side;

            Debug::g_movement.on_ground = on_ground;
            Debug::g_movement.jump_held = space;
            Debug::g_movement.calls++;

            // ── BHOP ─────────────────────────────────────────────────────────
            {
                bool just_landed = on_ground && !s_was_on_ground;
                bool in_air = !on_ground;

                if (g_cfg.misc.movement.bhop && space) {
                    uintptr_t client = Memory::GetModuleBase("client.dll");
                    if (client) {
                        if (just_landed) {
                            s_landing_ticks = kLandingWindow;
                        }

                        if (s_landing_ticks > 0) {
                            Memory::Write(client + kForceJumpRVA, kJumpOn);
                            --s_landing_ticks;
                            g_hook_alive.store(true, std::memory_order_release);
                            g_cv.notify_one();
                        }
                        else if (in_air) {
                            Memory::Write(client + kForceJumpRVA, kJumpOff);
                            g_hook_alive.store(false, std::memory_order_release);
                        }
                    }
                    Debug::g_movement.bhop_active = (s_landing_ticks > 0) ? 1 : 0;
                }
                else {
                    s_landing_ticks = 0;
                    g_hook_alive.store(false, std::memory_order_release);
                    g_cv.notify_one();
                }
            }

            bool moving_now = (std::fabs(orig_fwd) > 0.1f) || (std::fabs(orig_side) > 0.1f);
            bool released = s_was_moving && !moving_now;

            // ── FAST STOP ────────────────────────────────────────────────────
            bool fast_stop_applied = false;

            if (g_cfg.misc.movement.fast_stop && on_ground && (released || s_stop_ticks > 0)) {
                auto [vx, vy, speed] = ReadVelocity2D(pawn);

                if (speed > kFastStopMinSpeed && s_stop_ticks < 2) {
                    float yaw = cmd->viewangles_y;
                    float vel_yaw = Rad2Deg(std::atan2(vy, vx));
                    float diff = NormalizeYaw(vel_yaw - yaw);
                    float rad = Deg2Rad(diff);

                    *fwd = -std::cos(rad) * kUnitsScale;
                    *side = -std::sin(rad) * kUnitsScale;
                    *up = 0.f;

                    ++s_stop_ticks;
                    fast_stop_applied = true;
                    Debug::g_movement.fast_active = 1;
                }
                else {
                    s_stop_ticks = 0;
                    Debug::g_movement.fast_active = 0;
                }
            }
            else {
                s_stop_ticks = 0;
            }

            // ── COUNTER-STRAFE ────────────────────────────────────────────────
            if (g_cfg.misc.movement.counter_strafe &&
                on_ground && released && !fast_stop_applied)
            {
                *fwd = -s_last_fwd;
                *side = -s_last_side;
                *up = 0.f;

                ++s_stop_ticks;
                fast_stop_applied = true;

                Debug::g_movement.counter_active = 1;
            }
            else if (!released) {
                Debug::g_movement.counter_active = 0;
            }

            // ── AUTO STRAFE ───────────────────────────────────────────────────
            if (g_cfg.misc.movement.auto_strafe && !on_ground) {
                auto [vx, vy, speed] = ReadVelocity2D(pawn);

                if (speed > kMinStrafeSpeed) {
                    float cur_yaw = cmd->viewangles_y;

                    float new_fwd = 0.f, new_side = 0.f;
                    ComputeStrafeWishdir(vx, vy, speed, cur_yaw, new_fwd, new_side);

                    *fwd = new_fwd;
                    *side = new_side;
                    *up = 0.f;

                    Debug::g_movement.fast_active = 0;
                    Debug::g_movement.counter_active = 0;
                }
            }

            s_last_fwd = orig_fwd;
            s_last_side = orig_side;
            s_was_moving = moving_now;
            s_was_on_ground = on_ground;
        }
        __except (FilterSEH(GetExceptionCode())) {
            Debug::g_movement.access_violation_count++;
        }
    }

    static void Worker() {
        static bool w_was_on_ground = false;
        static int  w_landing_ticks = 0;

        while (g_running.load(std::memory_order_acquire)) {
            {
                std::unique_lock<std::mutex> lk(g_mtx);
                g_cv.wait_for(lk, std::chrono::milliseconds(8), [] {
                    return !g_running.load(std::memory_order_relaxed)
                        || !g_hook_alive.load(std::memory_order_relaxed);
                    });
            }

            if (!g_running.load(std::memory_order_relaxed)) break;

            if (g_hook_alive.load(std::memory_order_acquire)) {
                w_was_on_ground = false;
                w_landing_ticks = 0;
                continue;
            }

            uintptr_t client = Memory::GetModuleBase("client.dll");
            uintptr_t pawn = SDK::GetLocalPlayerPawn();
            if (!client || !pawn) continue;
            if (!PawnAlive(pawn))  continue;

            uint32_t flags = *reinterpret_cast<uint32_t*>(pawn + k_m_fFlags);
            bool     on_ground = (flags & kFL_ONGROUND) != 0;
            bool     space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            bool     just_landed = on_ground && !w_was_on_ground;

            if (g_cfg.misc.movement.bhop && space) {
                if (just_landed)        w_landing_ticks = kLandingWindow;

                if (w_landing_ticks > 0) {
                    Memory::Write(client + kForceJumpRVA, kJumpOn);
                    --w_landing_ticks;
                }
                else if (!on_ground) {
                    Memory::Write(client + kForceJumpRVA, kJumpOff);
                }
            }
            else {
                w_landing_ticks = 0;
                Memory::Write(client + kForceJumpRVA, kJumpOff);
            }

            w_was_on_ground = on_ground;
        }
    }

    void Init() {
        g_running.store(true, std::memory_order_release);
        g_thread = std::thread(Worker);
    }

    void Shutdown() {
        g_running.store(false, std::memory_order_release);
        g_cv.notify_all();
        if (g_thread.joinable()) g_thread.join();
    }

} // namespace Features::Misc::Bhop