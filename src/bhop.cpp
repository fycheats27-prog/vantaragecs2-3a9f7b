// language: C++, file: src/bhop.cpp
// *Bhop tick-synced vía CreateMove + counter-strafe + fast-stop. Sin offsets.h.*
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
#include <chrono>

namespace Features::Misc::Bhop {

    // ── RVAs client.dll ──
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

    // ── schema CUserCmd — DOS saltos ──
    // CUserCmd --0x40--> CCSGOUserCmdPB --0x00--> CBaseUserCmdPB
    static constexpr int k_m_csgoUserCmd_base = 0x40;
    static constexpr int k_pBaseCmd_offset = 0x00;

    // ── schema CBaseUserCmdPB ──
    static constexpr int k_viewangles_pb = 0x208;   // CMsgQAngle inline
    static constexpr int k_forwardmove = 0x58;    // float
    static constexpr int k_leftmove = 0x5C;    // float (sidemove)
    static constexpr int k_upmove = 0x60;    // float

    // ── schema CMsgQAngle ──
    static constexpr int k_qa_yaw = 0x04;

    static constexpr float kPI = 3.14159265358979f;
    static inline float Rad2Deg(float r) { return r * (180.0f / kPI); }

    static inline float NormalizeYaw(float a) {
        while (a > 180.0f)  a -= 360.0f;
        while (a < -180.0f) a += 360.0f;
        return a;
    }

    // ── estado de flanco, un jugador local, un tick ──
    static float s_last_fwd = 0.f;
    static float s_last_side = 0.f;
    static bool  s_was_moving = false;

    // ── fallback: solo corre si el hook de CreateMove no enganchó ──
    static std::atomic<bool> g_running{ false };
    static std::thread       g_thread;

    // ═══════════════════════════════════════════════════════════════
    // ENTRY DESDE CreateMove (tick exacto)
    // ═══════════════════════════════════════════════════════════════
    void OnCreateMove(void* pCmd) {
        if (!pCmd) return;

        uintptr_t pawn = SDK::GetLocalPlayerPawn();
        if (!pawn) return;

        __try {
            int      hp = *reinterpret_cast<int*>(pawn + k_m_iHealth);
            uint8_t  life = *reinterpret_cast<uint8_t*>(pawn + k_m_lifeState);
            if (hp <= 0 || life != 0) return;

            // wrapper -> base. Aquí estaba el bug anterior: faltaba el primer salto.
            uintptr_t csgo_pb = *reinterpret_cast<uintptr_t*>((uintptr_t)pCmd + k_m_csgoUserCmd_base);
            if (csgo_pb < 0x10000) return;
            uintptr_t base_pb = *reinterpret_cast<uintptr_t*>(csgo_pb + k_pBaseCmd_offset);
            if (base_pb < 0x10000) return;

            uint32_t flags = *reinterpret_cast<uint32_t*>(pawn + k_m_fFlags);
            bool     on_ground = (flags & kFL_ONGROUND) != 0;
            bool     space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

            float* fwd = reinterpret_cast<float*>(base_pb + k_forwardmove);
            float* side = reinterpret_cast<float*>(base_pb + k_leftmove);
            float* up = reinterpret_cast<float*>(base_pb + k_upmove);

            float orig_fwd = *fwd;
            float orig_side = *side;

            Debug::g_bhop.on_ground = on_ground;
            Debug::g_bhop.jump_held = space;
            Debug::g_bhop.flags = flags;
            Debug::g_bhop.tick++;

            // ── BHOP ──
            // dwForceJump escrito en el tick, no en un hilo con Sleep(2).
            if (g_cfg.misc.movement.bhop && space) {
                uintptr_t client = Memory::GetModuleBase("client.dll");
                if (client) {
                    Memory::Write(client + kForceJumpRVA, on_ground ? kJumpOn : kJumpOff);
                }
                Debug::g_bhop.bhop_active = on_ground ? 1 : 0;
            }

            bool moving_now = (orig_fwd != 0.f) || (orig_side != 0.f);
            bool released = s_was_moving && !moving_now;

            // ── FAST STOP ──
            // Tick de release: cero inputs, la fricción del juego frena sola.
            if (g_cfg.misc.movement.fast_stop && on_ground && released) {
                *fwd = 0.f;
                *side = 0.f;
                Debug::g_bhop.fast_active = 1;
            }

            // ── COUNTER-STRAFE ──
            // Mismo tick: empujar exactamente al revés del input que acabas de soltar.
            // Un solo tick. Mantenerlo mientras la tecla está pulsada te manda hacia atrás.
            if (g_cfg.misc.movement.counter_strafe && on_ground && released) {
                *fwd = -s_last_fwd;
                *side = -s_last_side;
                Debug::g_bhop.counter_active = 1;
            }

            // ── AUTO STRAFE (tu código, intacto) ──
            if (g_cfg.misc.movement.auto_strafe && !on_ground) {
                Vec3  vel = *reinterpret_cast<Vec3*>(pawn + k_m_vecAbsVelocity);
                float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);

                if (speed > 5.0f) {
                    uintptr_t pViewAngles = base_pb + k_viewangles_pb;
                    float cur_yaw = *reinterpret_cast<float*>(pViewAngles + k_qa_yaw);

                    float vel_yaw = Rad2Deg(atan2f(vel.y, vel.x));
                    float ideal = Rad2Deg(atanf(30.0f / speed));
                    if (ideal < 1.0f)  ideal = 1.0f;
                    if (ideal > 60.0f) ideal = 60.0f;

                    float diff = NormalizeYaw(cur_yaw - vel_yaw);
                    bool  strafe_left = (diff > 0.0f);

                    float target_yaw = NormalizeYaw(vel_yaw + (strafe_left ? -ideal : ideal));
                    float delta = NormalizeYaw(target_yaw - cur_yaw);
                    float step = delta * 0.9f;
                    if (step > 45.0f)  step = 45.0f;
                    if (step < -45.0f) step = -45.0f;

                    *reinterpret_cast<float*>(pViewAngles + k_qa_yaw) = NormalizeYaw(cur_yaw + step);

                    *side = (strafe_left ? -1.0f : 1.0f) * 450.0f;
                    *fwd = 0.0f;
                    *up = 0.0f;
                }
            }

            s_last_fwd = orig_fwd;
            s_last_side = orig_side;
            s_was_moving = moving_now;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // ═══════════════════════════════════════════════════════════════
    // FALLBACK — solo si CreateMove no enganchó
    // ═══════════════════════════════════════════════════════════════
    static void Worker() {
        while (g_running.load(std::memory_order_acquire)) {
            if (!Debug::g_bhop.hook_alive) {
                uintptr_t client = Memory::GetModuleBase("client.dll");
                uintptr_t pawn = SDK::GetLocalPlayerPawn();
                if (client && pawn) {
                    int      hp = *reinterpret_cast<int*>(pawn + k_m_iHealth);
                    uint8_t  life = *reinterpret_cast<uint8_t*>(pawn + k_m_lifeState);
                    uint32_t flags = *reinterpret_cast<uint32_t*>(pawn + k_m_fFlags);
                    bool on_ground = (flags & kFL_ONGROUND) != 0;
                    bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                    bool alive = (hp > 0 && life == 0);

                    if (g_cfg.misc.movement.bhop && space && on_ground && alive) {
                        Memory::Write(client + kForceJumpRVA, kJumpOn);
                    }
                    else {
                        Memory::Write(client + kForceJumpRVA, kJumpOff);
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
    }

} // namespace Features::Misc::Bhop