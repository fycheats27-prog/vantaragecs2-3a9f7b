// language: C++, file: src/esp.cpp
// ═══════════════════════════════════════════════════════════════════════════
//  ESP + NadePred + GrenadeHelper — sin framework externo, solo offsets locales
//  GrenadeHelper: dos bugs del original corregidos (eyeLift sin gate de velocidad,
//  kind re-clasificado al primer pin-pull).
//  NadePred: física 1/128 + CGameTrace offsets 0x84/0x90/0xAC + filtro correcto.
// ═══════════════════════════════════════════════════════════════════════════
#include "esp.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include "debug.h"
#include <imgui.h>
#include <windows.h>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")

namespace Features::Visuals::ESP {
    namespace {
        // ═══════════════════════════════════════════════════════════════════
        //  Common helpers
        // ═══════════════════════════════════════════════════════════════════
        struct Vec3 {
            float x = 0.f, y = 0.f, z = 0.f;
            Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
            Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
            Vec3 operator*(float s)      const { return { x * s,   y * s,   z * s }; }
            float dot(const Vec3& o)     const { return x * o.x + y * o.y + z * o.z; }
            float length()               const { return std::sqrt(x * x + y * y + z * z); }
            Vec3 normalized() const {
                float l = length();
                return l > 1e-6f ? Vec3{ x / l, y / l, z / l } : Vec3{ 0.f, 0.f, 1.f };
            }
        };
        struct Vec2 { float x = 0.f, y = 0.f; };
        struct Mat4 { float m[4][4]{}; };

        template<typename T>
        static T SafeRead(uintptr_t addr) {
            if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T{};
            __try { return *reinterpret_cast<T*>(addr); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
        }
        static bool SafeMemReadBuf(uintptr_t addr, void* out, size_t n) {
            if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) { std::memset(out, 0, n); return false; }
            __try { std::memcpy(out, reinterpret_cast<void*>(addr), n); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { std::memset(out, 0, n); return false; }
        }
        static bool ValidPtr(uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFF; }
        template<typename T>
        static void SafeWrite(uintptr_t addr, T val) {
            if (!ValidPtr(addr)) return;
            __try { *reinterpret_cast<T*>(addr) = val; }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        static bool IsFinite(const Vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
                && !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
        }
        static float Dist(const Vec3& a, const Vec3& b) {
            float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        static bool ToScreen(const Vec3& w, Vec2& out, const Mat4& vm, int W, int H) {
            float cw = vm.m[3][0] * w.x + vm.m[3][1] * w.y + vm.m[3][2] * w.z + vm.m[3][3];
            if (!std::isfinite(cw) || cw < 0.001f) return false;
            float cx = vm.m[0][0] * w.x + vm.m[0][1] * w.y + vm.m[0][2] * w.z + vm.m[0][3];
            float cy = vm.m[1][0] * w.x + vm.m[1][1] * w.y + vm.m[1][2] * w.z + vm.m[1][3];
            float inv = 1.f / cw;
            out.x = (W * 0.5f) * (cx * inv + 1.f);
            out.y = (H * 0.5f) * (1.f - cy * inv);
            return std::isfinite(out.x) && std::isfinite(out.y);
        }
        static ImU32 ToCol(const float c[4]) {
            return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), (int)(c[3] * 255));
        }
        static uint32_t ColorARGB(const float c[4]) {
            return ((uint32_t)(c[3] * 255) << 24) | ((uint32_t)(c[0] * 255) << 16)
                | ((uint32_t)(c[1] * 255) << 8) | (uint32_t)(c[2] * 255);
        }

        // ═══════════════════════════════════════════════════════════════════
        //  Schema / Offsets / Bones
        // ═══════════════════════════════════════════════════════════════════
        namespace Schema {
            constexpr int m_iHealth = 0x34C, m_iTeamNum = 0x3E7, m_lifeState = 0x354;
            constexpr int m_pGameSceneNode = 0x330, m_vecAbsVelocity = 0x3F8, m_vOldOrigin = 0x14A4;
            constexpr int m_hPlayerPawn = 0x92C, m_pWeaponServices = 0x12F0, m_ArmorValue = 0x1EC8;
            constexpr int m_bIsScoped = 0x1E98, m_sSanitizedPlayerName = 0x878, m_iszPlayerName = 0x6FC;
            constexpr int m_hActiveWeapon = 0x60, m_bBombTicking = 0x1288, m_flC4Blow = 0x12B8;
            constexpr int m_nBombSite = 0x128C, m_bBeingDefused = 0x12C4, m_flDefuseCountDown = 0x12D8;
            constexpr int m_Identity = 0x10, m_designerName = 0x20, m_vecAbsOrigin = 0xC8, m_bDormant = 0x103;
            constexpr int m_modelState = 0x140, m_pBoneArray = 0x80;
            constexpr int m_Glow = 0xDE8, m_iGlowType = 0x30, m_glowColorOverride = 0x40, m_bGlowing = 0x51;
            constexpr int m_flThrowStrength = 0x1F30, m_flTotalRoundDamageDealt = 0x130;
            constexpr int m_vecStashedGrenadeThrowPosition = 0x1F2C, m_bGrenadeParametersStashed = 0x1F1C;
            // Granade helper additions
            constexpr int m_fFlags = 0x3F4;
            constexpr int m_bPinPulled = 0x1F23, m_bIsHeldByPlayer = 0x1F22, m_fThrowTime = 0x1F28;
        }
        namespace Offsets {
            constexpr uintptr_t dwEntityList = 0x271A908, dwLocalPlayerPawn = 0x2565718;
            constexpr uintptr_t dwLocalPlayerController = 0x253CB78, dwViewMatrix = 0x256B020;
            constexpr uintptr_t dwViewAngles = 0x257BCC8, dwGlobalVars = 0x2230090;
            constexpr uintptr_t dwPlantedC4 = 0x24CB3C0;
            constexpr int globalvars_mapname = 0x188;
        }
        namespace Bones {
            enum : int {
                PELVIS = 1, SPINE_0 = 2, SPINE_1 = 3, SPINE_2 = 4, NECK = 6, HEAD = 7,
                CLAV_L = 8, ARM_UP_L = 9, ARM_LO_L = 10, HAND_L = 11, CLAV_R = 12, ARM_UP_R = 13, ARM_LO_R = 14, HAND_R = 15,
                HIP_L = 17, KNEE_L = 18, ANKLE_L = 19, HIP_R = 20, KNEE_R = 21, ANKLE_R = 22, CHEST = 23
            };
            constexpr int MAX = 24;
            struct Joint { Vec3 pos; uint8_t pad[0x14]; };
            static_assert(sizeof(Joint) == 0x20);
            static const int links[][2] = {
                {HEAD,NECK},{NECK,CHEST},{CHEST,SPINE_2},{SPINE_2,SPINE_1},{SPINE_1,PELVIS},
                {CHEST,CLAV_L},{CLAV_L,ARM_UP_L},{ARM_UP_L,ARM_LO_L},{ARM_LO_L,HAND_L},
                {CHEST,CLAV_R},{CLAV_R,ARM_UP_R},{ARM_UP_R,ARM_LO_R},{ARM_LO_R,HAND_R},
                {PELVIS,HIP_L},{HIP_L,KNEE_L},{KNEE_L,ANKLE_L},{PELVIS,HIP_R},{HIP_R,KNEE_R},{KNEE_R,ANKLE_R}
            };
            constexpr int LINK_COUNT = (int)(sizeof(links) / sizeof(links[0]));
        }

        // ═══════════════════════════════════════════════════════════════════
        //  Player / DmgMarker
        // ═══════════════════════════════════════════════════════════════════
        struct Player {
            uintptr_t pawn = 0, controller = 0;
            int index = 0, team = 0, health = 0, armor = 0;
            float distance = 0.f;
            Vec3 origin{}, head{}, feet{}, bones[Bones::MAX]{};
            char name[64]{}, weapon[32]{};
            bool scoped = false;
        };
        struct DmgMarker { Vec3 pos{}; float time = 0.f; int dmg = 0; };
        static std::vector<DmgMarker> g_dmg;

        // ═══════════════════════════════════════════════════════════════════
        //  Utility helpers
        // ═══════════════════════════════════════════════════════════════════
        static void Sanitize(char* s, size_t n) {
            if (!s || !n) return; s[n - 1] = 0;
            for (size_t i = 0; s[i]; ++i) if ((unsigned char)s[i] < 0x20) s[i] = ' ';
        }
        static void DesignerName(uintptr_t ent, char* out, size_t n) {
            if (!out || !n) return; out[0] = 0;
            if (!ValidPtr(ent)) return;
            uintptr_t id = SafeRead<uintptr_t>(ent + Schema::m_Identity);
            if (!ValidPtr(id)) return;
            uintptr_t str = SafeRead<uintptr_t>(id + Schema::m_designerName);
            if (!ValidPtr(str)) return;
            SafeMemReadBuf(str, out, n - 1); out[n - 1] = 0;
        }
        static const char* WeaponDisplay(const char* raw) {
            if (!raw || !raw[0]) return "";
            const char* s = raw;
            if (std::strncmp(s, "weapon_", 7) == 0) s += 7;
            if (!std::strcmp(s, "hkp2000")) return "P2000";
            if (!std::strcmp(s, "usp_silencer")) return "USP-S";
            if (!std::strcmp(s, "glock")) return "Glock";
            if (!std::strcmp(s, "deagle")) return "Deagle";
            if (!std::strcmp(s, "elite")) return "Dualies";
            if (!std::strcmp(s, "ak47")) return "AK-47";
            if (!std::strcmp(s, "m4a1")) return "M4A4";
            if (!std::strcmp(s, "m4a1_silencer")) return "M4A1-S";
            if (!std::strcmp(s, "awp")) return "AWP";
            if (!std::strcmp(s, "ssg08")) return "Scout";
            if (std::strstr(s, "knife")) return "Knife";
            if (!std::strcmp(s, "c4")) return "C4";
            if (std::strstr(s, "hegrenade")) return "HE";
            if (std::strstr(s, "flashbang")) return "Flash";
            if (std::strstr(s, "smokegrenade")) return "Smoke";
            if (std::strstr(s, "molotov") || std::strstr(s, "incgrenade")) return "Molotov";
            if (std::strstr(s, "decoy")) return "Decoy";
            return s;
        }
        static ImU32 HpGradient(int hp) {
            float t = std::clamp(hp / 100.f, 0.f, 1.f);
            float r = t < 0.5f ? 1.f : (1.f - t) * 2.f;
            float g = t < 0.5f ? t * 2.f : 1.f;
            return IM_COL32((int)(r * 255), (int)(g * 255), 50, 255);
        }
        static void ApplyGlow(uintptr_t ent, const float col[4]) {
            if (!ValidPtr(ent)) return;
            uintptr_t glow = ent + Schema::m_Glow;
            SafeWrite<int>(glow + Schema::m_iGlowType, 3);
            SafeWrite<int>(glow + 0x34, -1);
            SafeWrite<int>(glow + 0x38, 10000);
            SafeWrite<int>(glow + 0x3C, 0);
            SafeWrite<uint32_t>(glow + Schema::m_glowColorOverride, ColorARGB(col));
            SafeWrite<uint8_t>(glow + Schema::m_bGlowing, 1);
        }
        static float ReadCurtime(uintptr_t client) {
            uintptr_t gv = SafeRead<uintptr_t>(client + Offsets::dwGlobalVars);
            return ValidPtr(gv) ? SafeRead<float>(gv + 0x2C) : 0.f;
        }
        static uintptr_t ResolvePlantedC4(uintptr_t client) {
            uintptr_t p1 = SafeRead<uintptr_t>(client + Offsets::dwPlantedC4);
            if (!ValidPtr(p1)) return 0;
            if (SafeRead<bool>(p1 + Schema::m_bBombTicking) || SafeRead<bool>(p1 + 0x12D0)) return p1;
            uintptr_t p2 = SafeRead<uintptr_t>(p1);
            if (ValidPtr(p2) && (SafeRead<bool>(p2 + Schema::m_bBombTicking) || SafeRead<bool>(p2 + 0x12D0))) return p2;
            return 0;
        }

        // ═══════════════════════════════════════════════════════════════════
        //  EntityList
        // ═══════════════════════════════════════════════════════════════════
        struct EntityList {
            std::vector<Player> players;
            int local_team = 0;
            void Update(uintptr_t local_pawn, uintptr_t entity_system) {
                players.clear(); players.reserve(32); local_team = 0;
                if (!ValidPtr(entity_system)) return;
                int max_index = SafeRead<int>(entity_system + 0x2120);
                if (max_index <= 0 || max_index > 1024) max_index = 64;
                Vec3 local_origin{};
                if (ValidPtr(local_pawn)) {
                    local_origin = SafeRead<Vec3>(local_pawn + Schema::m_vOldOrigin);
                    local_team = SafeRead<uint8_t>(local_pawn + Schema::m_iTeamNum);
                }
                for (int i = 1; i <= max_index && i < 64; ++i) {
                    uintptr_t chunk = SafeRead<uintptr_t>(entity_system + 0x10 + 8 * ((i & 0x7FFF) >> 9));
                    if (!ValidPtr(chunk)) continue;
                    uintptr_t controller = SafeRead<uintptr_t>(chunk + 0x70 * (i & 0x1FF));
                    if (!ValidPtr(controller)) continue;
                    uint32_t pawn_handle = SafeRead<uint32_t>(controller + Schema::m_hPlayerPawn);
                    if (!pawn_handle || pawn_handle == 0xFFFFFFFF) continue;
                    uintptr_t pchunk = SafeRead<uintptr_t>(entity_system + 0x10 + 8 * ((pawn_handle & 0x7FFF) >> 9));
                    if (!ValidPtr(pchunk)) continue;
                    uintptr_t pawn = SafeRead<uintptr_t>(pchunk + 0x70 * (pawn_handle & 0x1FF));
                    if (!ValidPtr(pawn) || pawn == local_pawn) continue;
                    int hp = SafeRead<int>(pawn + Schema::m_iHealth);
                    if (hp <= 0 || hp > 100) continue;
                    int team = SafeRead<uint8_t>(pawn + Schema::m_iTeamNum);
                    if (team != 2 && team != 3) continue;
                    if (SafeRead<uint8_t>(pawn + Schema::m_lifeState) != 0) continue;
                    uintptr_t node = SafeRead<uintptr_t>(pawn + Schema::m_pGameSceneNode);
                    if (!ValidPtr(node) || SafeRead<bool>(node + Schema::m_bDormant)) continue;
                    uintptr_t bone_arr = SafeRead<uintptr_t>(node + Schema::m_modelState + Schema::m_pBoneArray);
                    if (!ValidPtr(bone_arr)) continue;
                    Bones::Joint joints[Bones::MAX]{};
                    SafeMemReadBuf(bone_arr, joints, sizeof(joints));
                    if (!IsFinite(joints[Bones::HEAD].pos)) continue;
                    Player p{};
                    p.pawn = pawn; p.controller = controller; p.index = i; p.team = team; p.health = hp;
                    p.armor = std::clamp(SafeRead<int>(pawn + Schema::m_ArmorValue), 0, 100);
                    p.origin = SafeRead<Vec3>(pawn + Schema::m_vOldOrigin);
                    p.scoped = SafeRead<bool>(pawn + Schema::m_bIsScoped);
                    if (!IsFinite(p.origin)) continue;
                    for (int b = 0; b < Bones::MAX; ++b) p.bones[b] = joints[b].pos;
                    p.head = joints[Bones::HEAD].pos; p.head.z += 6.f;
                    Vec3 al = joints[Bones::ANKLE_L].pos, ar = joints[Bones::ANKLE_R].pos;
                    bool vl = IsFinite(al), vr = IsFinite(ar);
                    if (vl && vr) p.feet = (al.z < ar.z) ? al : ar;
                    else if (vl) p.feet = al; else if (vr) p.feet = ar;
                    else { p.feet = joints[Bones::PELVIS].pos; p.feet.z -= 34.f; }
                    p.feet.z -= 2.f;
                    uintptr_t name_ptr = SafeRead<uintptr_t>(controller + Schema::m_sSanitizedPlayerName);
                    if (ValidPtr(name_ptr)) { SafeMemReadBuf(name_ptr, p.name, sizeof(p.name) - 1); Sanitize(p.name, sizeof(p.name)); }
                    if (!p.name[0]) { SafeMemReadBuf(controller + Schema::m_iszPlayerName, p.name, sizeof(p.name) - 1); Sanitize(p.name, sizeof(p.name)); }
                    uintptr_t ws = SafeRead<uintptr_t>(pawn + Schema::m_pWeaponServices);
                    if (ValidPtr(ws)) {
                        uint32_t h_w = SafeRead<uint32_t>(ws + Schema::m_hActiveWeapon);
                        if (h_w && h_w != 0xFFFFFFFF) {
                            uintptr_t wc = SafeRead<uintptr_t>(entity_system + 0x10 + 8 * ((h_w & 0x7FFF) >> 9));
                            if (ValidPtr(wc)) {
                                uintptr_t weap = SafeRead<uintptr_t>(wc + 0x70 * (h_w & 0x1FF));
                                if (ValidPtr(weap)) {
                                    char d[64]{}; DesignerName(weap, d, sizeof(d));
                                    std::snprintf(p.weapon, sizeof(p.weapon), "%s", WeaponDisplay(d));
                                }
                            }
                        }
                    }
                    p.distance = Dist(local_origin, p.origin);
                    bool enemy = (team != local_team);
                    if (g_cfg.visuals.player.glow) {
                        const float* gc = enemy ? g_cfg.visuals.player.glow_enemy : g_cfg.visuals.player.glow_team;
                        ApplyGlow(pawn, gc);
                    }
                    players.push_back(p);
                }
            }
        };

        // ═══════════════════════════════════════════════════════════════════
        //  ESP draw helpers
        // ═══════════════════════════════════════════════════════════════════
        static void DrawBombTimer(ImDrawList* dl, int W, uintptr_t client) {
            if (!g_cfg.visuals.world.bomb_timer && !g_cfg.visuals.world.bomb_esp) return;
            uintptr_t c4 = ResolvePlantedC4(client);
            if (!ValidPtr(c4) || !SafeRead<bool>(c4 + Schema::m_bBombTicking)) return;
            float blow = SafeRead<float>(c4 + Schema::m_flC4Blow);
            float cur = ReadCurtime(client);
            float rem = blow - cur;
            if (rem <= 0.f || rem > 45.f) return;
            bool defusing = SafeRead<bool>(c4 + Schema::m_bBeingDefused);
            float def_rem = defusing ? (SafeRead<float>(c4 + Schema::m_flDefuseCountDown) - cur) : -1.f;
            int site = SafeRead<int>(c4 + Schema::m_nBombSite);
            if (g_cfg.visuals.world.bomb_timer) {
                bool crit = rem < 5.f;
                float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * (crit ? 12.f : 2.5f));
                float CW = 240.f, CH = defusing ? 72.f : 54.f;
                float cx = W * 0.5f, cy = 58.f;
                ImVec2 p0(cx - CW * 0.5f, cy), p1(cx + CW * 0.5f, cy + CH);
                dl->AddRectFilled(p0, p1, IM_COL32(8, 10, 16, 240), 6.f);
                dl->AddRect(p0, p1, crit ? IM_COL32(220, 60, 50, 255) : IM_COL32(40, 140, 240, 255),
                    6.f, 0, 1.4f + pulse * (crit ? 1.5f : 0.f));
                float bw = CW - 16.f, bx = cx - bw * 0.5f, by = cy + 8.f;
                float prog = std::clamp(rem / 40.f, 0.f, 1.f);
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + 7.f), IM_COL32(30, 34, 42, 255), 3.f);
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * prog, by + 7.f),
                    crit ? IM_COL32(230, 55, 45, 255) : IM_COL32(40, 145, 245, 255), 3.f);
                char buf[48]; std::snprintf(buf, sizeof(buf), "C4  %s   %.1fs", site == 1 ? "B" : "A", rem);
                ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(cx - ts.x * 0.5f, by + 12.f),
                    crit ? IM_COL32(255, 90, 80, 255) : IM_COL32(235, 240, 250, 255), buf);
                if (defusing && def_rem > 0.f) {
                    char d[32]; std::snprintf(d, sizeof(d), "defusing  %.1fs", def_rem);
                    ImVec2 ds = ImGui::CalcTextSize(d);
                    dl->AddText(ImVec2(cx - ds.x * 0.5f, by + 28.f), IM_COL32(80, 230, 150, 255), d);
                }
            }
            if (g_cfg.visuals.player.glow_bomb)
                ApplyGlow(c4, g_cfg.visuals.player.glow_bomb_color);
        }

        static void DrawRadar(ImDrawList* dl, int W, const std::vector<Player>& players, const Vec3& local_origin,
            int local_team, const Mat4& vm, int cfg_pos, int cfg_zoom) {
            if (!IsFinite(local_origin)) return;
            constexpr float BASE = 260.f;
            float zoom = 1.f + cfg_zoom * 0.01f, RANGE = 6500.f / zoom, PADDING = 16.f, RS = BASE;
            float rx = (cfg_pos == 0) ? PADDING : (cfg_pos == 2) ? (W - RS) * 0.5f : W - RS - PADDING;
            float ry = PADDING, cx = rx + RS * 0.5f, cy = ry + RS * 0.5f, scale = (RS * 0.46f) / RANGE;
            float yaw = std::atan2(-vm.m[1][2], -vm.m[0][2]);
            dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + RS, ry + RS), IM_COL32(12, 14, 18, 240), 8.f);
            dl->AddRect(ImVec2(rx, ry), ImVec2(rx + RS, ry + RS), IM_COL32(50, 160, 230, 200), 8.f, 0, 1.8f);
            float dx = std::cos(yaw), dy = -std::sin(yaw);
            dl->AddTriangleFilled(ImVec2(cx + dx * 10.f, cy + dy * 10.f),
                ImVec2(cx - dy * 6.5f - dx * 5.5f, cy + dx * 6.5f - dy * 5.5f),
                ImVec2(cx + dy * 6.5f - dx * 5.5f, cy - dx * 6.5f - dy * 5.5f),
                IM_COL32(100, 235, 255, 255));
            for (const auto& p : players) {
                float ddx = p.origin.x - local_origin.x, ddy = p.origin.y - local_origin.y;
                if (std::sqrt(ddx * ddx + ddy * ddy) > RANGE) continue;
                float px = std::clamp(cx + ddx * scale, rx + 8.f, rx + RS - 8.f);
                float py = std::clamp(cy - ddy * scale, ry + 8.f, ry + RS - 8.f);
                ImU32 col = (p.team == local_team) ? IM_COL32(70, 220, 140, 255) : IM_COL32(245, 75, 75, 255);
                dl->AddCircleFilled(ImVec2(px, py), 4.5f, col, 16);
            }
            dl->AddText(ImVec2(rx + 10.f, ry + 7.f), IM_COL32(170, 215, 255, 230), "RADAR");
        }

        static void DrawDroppedWeapons(ImDrawList* dl, int W, int H, const Mat4& vm, uintptr_t entity_system, uintptr_t local_pawn) {
            if (!g_cfg.visuals.world.dropped_weapons && !g_cfg.visuals.player.glow_weapons) return;
            if (!ValidPtr(entity_system)) return;
            int max_index = SafeRead<int>(entity_system + 0x2120);
            if (max_index <= 0 || max_index > 2048) max_index = 1024;
            Vec3 local_origin{};
            if (ValidPtr(local_pawn)) local_origin = SafeRead<Vec3>(local_pawn + Schema::m_vOldOrigin);
            for (int i = 1; i <= max_index; ++i) {
                uintptr_t chunk = SafeRead<uintptr_t>(entity_system + 0x10 + (i >> 9) * 8);
                if (!ValidPtr(chunk)) continue;
                uintptr_t ent = SafeRead<uintptr_t>(chunk + (i & 0x1FF) * 0x70);
                if (!ValidPtr(ent) || ent == local_pawn) continue;
                uint32_t owner = SafeRead<uint32_t>(ent + 0x520);
                if (owner != 0xFFFFFFFF && owner != 0) continue;
                char d[64]{}; DesignerName(ent, d, sizeof(d));
                if (std::strncmp(d, "weapon_", 7) != 0) continue;
                uintptr_t node = SafeRead<uintptr_t>(ent + Schema::m_pGameSceneNode);
                if (!ValidPtr(node) || SafeRead<bool>(node + Schema::m_bDormant)) continue;
                Vec3 origin = SafeRead<Vec3>(node + Schema::m_vecAbsOrigin);
                if (!IsFinite(origin)) continue;
                if (g_cfg.visuals.player.glow_weapons) ApplyGlow(ent, g_cfg.visuals.player.glow_weapon_color);
                if (!g_cfg.visuals.world.dropped_weapons) continue;
                Vec3 above = origin; above.z += 12.f;
                Vec2 sp; if (!ToScreen(above, sp, vm, W, H)) continue;
                if (sp.x < 0.f || sp.x > W || sp.y < 0.f || sp.y > H) continue;
                float dist = Dist(local_origin, origin) * 0.0254f;
                char label[80]; std::snprintf(label, sizeof(label), "%s  %.0fm", d + 7, dist);
                ImVec2 ts = ImGui::CalcTextSize(label);
                float tx = std::clamp(sp.x - ts.x * 0.5f, 2.f, (float)W - ts.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, sp.y + 1.f), IM_COL32(0, 0, 0, 160), label);
                dl->AddText(ImVec2(tx, sp.y), IM_COL32(240, 210, 100, 255), label);
            }
        }

        static void DrawDamageMarkers(ImDrawList* dl, int W, int H, const Mat4& vm) {
            if (!g_cfg.visuals.world.damage_markers) return;
            float now = (float)ImGui::GetTime();
            constexpr float LIFE = 1.4f;
            for (auto it = g_dmg.begin(); it != g_dmg.end(); ) {
                float age = now - it->time;
                if (age > LIFE) { it = g_dmg.erase(it); continue; }
                float t = age / LIFE, alpha = 1.f - t;
                Vec3 up = it->pos; up.z += 18.f + t * 28.f;
                Vec2 sp;
                if (ToScreen(up, sp, vm, W, H)) {
                    char buf[16]; std::snprintf(buf, sizeof(buf), "-%d", it->dmg);
                    ImVec2 ts = ImGui::CalcTextSize(buf);
                    float tx = sp.x - ts.x * 0.5f;
                    dl->AddText(ImVec2(tx + 1.f, sp.y + 1.f), IM_COL32(0, 0, 0, (int)(alpha * 160)), buf);
                    dl->AddText(ImVec2(tx, sp.y), IM_COL32(255, 80, 60, (int)(alpha * 255)), buf);
                }
                ++it;
            }
        }

        static void DrawNoScopeOverlay(ImDrawList* dl, int W, int H, bool is_scoped) {
            if (!is_scoped || !g_cfg.visuals.world.no_scope || !dl) return;

            const float cx = W * 0.5f;
            const float cy = H * 0.5f;

            // ── Bars (Top & Bottom) ──────────────────────────────────────────────────
            // Thicker bars for a more cinematic feel, with a subtle gradient effect via alpha.
            const float barHeight = H * 0.14f;
            const ImU32 barColor = IM_COL32(10, 10, 12, 230); // Deep dark grey

            dl->AddRectFilled(ImVec2(0.f, 0.f), ImVec2((float)W, barHeight), barColor);
            dl->AddRectFilled(ImVec2(0.f, H - barHeight), ImVec2((float)W, (float)H), barColor);

            // ── Crosshair Geometry ───────────────────────────────────────────────────
            const float gap = 6.0f;
            const float length = 22.0f;
            const float thickness = 2.0f;

            // Colors
            const ImU32 outlineColor = IM_COL32(0, 0, 0, 200);
            const ImU32 mainColor = IM_COL32(240, 240, 245, 255); // Crisp white
            const ImU32 accentColor = IM_COL32(255, 60, 80, 255); // CurioT3ch Red

            // Helper to draw a line with outline
            auto DrawLineWithOutline = [&](const ImVec2& a, const ImVec2& b) {
                dl->AddLine(a, b, outlineColor, thickness + 1.5f);
                dl->AddLine(a, b, mainColor, thickness);
                };

            // Horizontal Lines
            DrawLineWithOutline(ImVec2(cx - gap - length, cy), ImVec2(cx - gap, cy));
            DrawLineWithOutline(ImVec2(cx + gap, cy), ImVec2(cx + gap + length, cy));

            // Vertical Lines
            DrawLineWithOutline(ImVec2(cx, cy - gap - length), ImVec2(cx, cy - gap));
            DrawLineWithOutline(ImVec2(cx, cy + gap), ImVec2(cx, cy + gap + length));

            // ── Center Dot (Tactical Marker) ─────────────────────────────────────────
            // A small diamond shape for precision aiming, accented in red.
            const float dotSize = 3.0f;
            const ImVec2 center = ImVec2(cx, cy);

            // Diamond vertices
            const ImVec2 p1 = ImVec2(cx, cy - dotSize);
            const ImVec2 p2 = ImVec2(cx + dotSize, cy);
            const ImVec2 p3 = ImVec2(cx, cy + dotSize);
            const ImVec2 p4 = ImVec2(cx - dotSize, cy);

            // Outline for the dot
            dl->AddQuad(p1, p2, p3, p4, outlineColor, 1.5f);
            // Filled accent
            dl->AddQuadFilled(p1, p2, p3, p4, accentColor);

            // Tiny white core for contrast
            const float coreSize = 1.2f;
            dl->AddRectFilled(
                ImVec2(cx - coreSize, cy - coreSize),
                ImVec2(cx + coreSize, cy + coreSize),
                IM_COL32(255, 255, 255, 255)
            );
        }

        // ═══════════════════════════════════════════════════════════════════
        //  NadePred — física 1/128 + CGameTrace (0x84/0x90/0xAC)
        // ═══════════════════════════════════════════════════════════════════
        enum class NadeKind : int {
            None = -1, Flash = 43, HE = 44, Smoke = 45, Molotov = 46, Decoy = 47, Incendiary = 48
        };
        struct NadeResult {
            static constexpr int kMaxPoints = 200;
            static constexpr int kMaxBounces = 24;
            Vec3 points[kMaxPoints];
            int pointCount = 0;
            Vec3 bounces[kMaxBounces];
            int bounceCount = 0;
            Vec3 endPos{};
            bool valid = false;
            float fuse = 0.f;
            void Reset() { pointCount = 0; bounceCount = 0; endPos = { 0,0,0 }; valid = false; fuse = 0.f; }
        };

        // CGameTrace offsets
        static constexpr int CT_ENDPOS = 0x84;
        static constexpr int CT_NORMAL = 0x90;
        static constexpr int CT_FRACTION = 0xAC;
        static constexpr int CT_SIZE = 0x110;
        static constexpr uint32_t MASK_GRENADE = 0x0200040B;

        static constexpr float kNadeSimDt = 1.0f / 128.0f;
        static constexpr int   kNadeTicksPerPt = 4;
        static constexpr float kNadeGravity = 320.0f;
        static constexpr float kNadeElasticity = 0.45f;
        static constexpr float kNadeMolotovDeg = 90.0f;
        static constexpr float kNadeMolotovFuse = 2.0f;
        static constexpr float kNadeSteepNormalZ = 0.7f;
        static constexpr float kNadeSteepSpeedSq = 96000.0f;
        static constexpr float kNadeHullRadius = 2.0f;
        static constexpr int   kNadeBounceLimit = 15;
        static constexpr int   kNadeSmokeTicks = 12;
        static constexpr float kNadeInheritScale = 1.25f;
        static constexpr int   kNadeForceResim = 20;
        static constexpr float kNadePushoutDist = 16.0f;
        static constexpr float kHeFlashFuseTotal = 1.625f;

        static constexpr ULONGLONG kHeldSimIntervalMs = 33;
        static constexpr ULONGLONG kInAirScanMs = 100;
        static constexpr ULONGLONG kInAirSimMs = 50;
        static constexpr ULONGLONG kInAirMaxLifeMs = 6000;

        static inline int NadeMaxTicks(NadeKind k) {
            switch (k) {
            case NadeKind::HE:        return 210;
            case NadeKind::Flash:     return 210;
            case NadeKind::Molotov:
            case NadeKind::Incendiary:return 290;
            case NadeKind::Smoke:
            case NadeKind::Decoy:     return 690;
            default:                  return 400;
            }
        }

#pragma pack(push, 1)
        struct CTraceFilter { void* vtable; uint32_t mask; uint32_t pad[3]; };
#pragma pack(pop)
        static void __fastcall FilterDtor(void*) {}
        static bool __fastcall FilterShouldHit(void*, void*, uint32_t) { return true; }
        static int  __fastcall FilterGetType(void*) { return 0; }
        static void* g_filter_vtable[4] = { (void*)FilterDtor, (void*)FilterShouldHit, (void*)FilterGetType, nullptr };

        struct Ray_t {
            Vec3 start; Vec3 end; Vec3 mins; Vec3 maxs;
            int32_t type; int32_t flags; uint8_t pad[0x8];
        };
        using TraceShapeFn = bool(__fastcall*)(void*, Ray_t*, Vec3*, Vec3*, void*, void*);
        static TraceShapeFn g_trace_shape = nullptr;
        static void* g_trace_mgr = nullptr;
        static bool         g_trace_resolved = false;
        static bool         g_trace_tried = false;

        static uint8_t* ScanPattern(uint8_t* base, size_t size, const char* pattern) {
            struct Byte { uint8_t val; bool wild; };
            Byte bytes[128]; size_t len = 0; const char* p = pattern;
            while (*p && len < 128) {
                while (*p == ' ') ++p; if (!*p) break;
                if (*p == '?') { bytes[len].val = 0; bytes[len].wild = true; len++; ++p; if (*p == '?') ++p; }
                else { bytes[len].val = (uint8_t)strtol(p, nullptr, 16); bytes[len].wild = false; len++; p += 2; }
            }
            if (len == 0 || size < len) return nullptr;
            for (size_t i = 0; i + len <= size; ++i) {
                bool match = true;
                for (size_t j = 0; j < len && match; ++j)
                    if (!bytes[j].wild && base[i + j] != bytes[j].val) match = false;
                if (match) return base + i;
            }
            return nullptr;
        }
        static void* ResolveManagerFromFn(uint8_t* fn) {
            __try {
                for (int i = 0; i < 80; ++i) {
                    uint8_t* cur = fn + i;
                    if (cur[0] == 0x48 && cur[1] == 0x8B && (cur[2] == 0x0D || cur[2] == 0x05 || cur[2] == 0x1D)) {
                        int32_t rel = *reinterpret_cast<int32_t*>(cur + 3);
                        void** ptr_to_mgr = reinterpret_cast<void**>(cur + 7 + rel);
                        if (!ValidPtr(reinterpret_cast<uintptr_t>(ptr_to_mgr))) continue;
                        void* mgr = *ptr_to_mgr;
                        if (ValidPtr(reinterpret_cast<uintptr_t>(mgr))) return mgr;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            return nullptr;
        }
        static void ResolveTrace(uintptr_t client) {
            if (g_trace_tried) return;
            g_trace_tried = true;
            if (!ValidPtr(client)) return;
            __try {
                HMODULE hmod = (HMODULE)client;
                MODULEINFO mi{};
                if (!GetModuleInformation(GetCurrentProcess(), hmod, &mi, sizeof(mi))) return;
                uint8_t* base = (uint8_t*)mi.lpBaseOfDll;
                size_t sz = mi.SizeOfImage;
                static const char* sigs[] = {
                    "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC 40 48 8B 0D",
                    "48 89 5C 24 ? 48 89 4C 24 ? 55 57",
                    "48 89 5C 24 20 48 89 4C 24 08 55 56 41",
                    "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 30 48 8B 0D",
                    nullptr
                };
                for (int i = 0; sigs[i]; ++i) {
                    uint8_t* hit = ScanPattern(base, sz, sigs[i]);
                    if (!hit) continue;
                    void* mgr = ResolveManagerFromFn(hit);
                    if (!mgr) continue;
                    g_trace_shape = (TraceShapeFn)hit;
                    g_trace_mgr = mgr;
                    g_trace_resolved = true;
                    return;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        static bool NadeTrace(const Vec3& start, const Vec3& end,
            Vec3& hit, Vec3& normal, float& frac) {
            if (!g_trace_resolved || !g_trace_shape || !g_trace_mgr) return false;
            CTraceFilter filter;
            filter.vtable = g_filter_vtable;
            filter.mask = MASK_GRENADE;
            std::memset(filter.pad, 0, sizeof(filter.pad));

            auto do_trace = [&](float hullR, Vec3& outHit, Vec3& outN, float& outF) -> bool {
                alignas(16) uint8_t ctrace[CT_SIZE]{};
                Vec3 s = start, e = end;
                Ray_t ray{}; ray.start = start; ray.end = end;
                ray.mins = { -hullR, -hullR, -hullR };
                ray.maxs = { hullR,  hullR,  hullR };
                ray.type = 0; ray.flags = 0; std::memset(ray.pad, 0, sizeof(ray.pad));
                bool hit_any = g_trace_shape(g_trace_mgr, &ray, &s, &e, &filter, ctrace);
                if (!hit_any) return false;
                float f = *reinterpret_cast<float*>(ctrace + CT_FRACTION);
                if (!std::isfinite(f) || f < 0.f || f >= 0.999f) return false;
                Vec3 h = *reinterpret_cast<Vec3*>(ctrace + CT_ENDPOS);
                Vec3 n = *reinterpret_cast<Vec3*>(ctrace + CT_NORMAL);
                if (!std::isfinite(h.x) || !std::isfinite(h.y) || !std::isfinite(h.z)) return false;
                if (!std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z)) return false;
                float nl = n.length();
                if (nl < 0.1f || nl > 1.5f) return false;
                n = n * (1.f / nl);
                Vec3 dir = (end - start).normalized();
                if (n.dot(dir) > 0.f) n = n * -1.f;
                outHit = h; outN = n; outF = f;
                return true;
                };

            __try {
                if (do_trace(kNadeHullRadius, hit, normal, frac)) return true;
                if (do_trace(0.f, hit, normal, frac)) return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            return false;
        }
        static Vec3 ClipVel(const Vec3& vel, const Vec3& n, float overbounce) {
            const float backoff = vel.dot(n);
            Vec3 out = vel - n * (backoff * overbounce);
            if (fabsf(out.x) < 0.1f) out.x = 0.f;
            if (fabsf(out.y) < 0.1f) out.y = 0.f;
            if (fabsf(out.z) < 0.1f) out.z = 0.f;
            return out;
        }
        static inline float MolotovSlopeZ() {
            return cosf(kNadeMolotovDeg * 3.14159265358979323846f / 180.f);
        }
        static bool NadeShouldDetonate(NadeKind kind, const Vec3& vel, int tick, float molotovFuse) {
            switch (kind) {
            case NadeKind::Smoke:
            case NadeKind::Decoy: {
                if (tick & 1) return false;
                const int t64 = tick >> 1;
                const float threshold = (kind == NadeKind::Smoke) ? 0.1f : 0.2f;
                const float speed2d = sqrtf(vel.x * vel.x + vel.y * vel.y);
                return speed2d < threshold && (t64 % kNadeSmokeTicks) == 0;
            }
            case NadeKind::Molotov:
            case NadeKind::Incendiary:
                return (float)tick * kNadeSimDt > molotovFuse;
            case NadeKind::Flash:
            case NadeKind::HE:
                return (float)(tick - 16) * kNadeSimDt > 1.5f;
            default: return false;
            }
        }
        static bool ResolveNadeCollision(const Vec3& hit_pos, const Vec3& hit_normal, float frac,
            Vec3& pos, Vec3& vel, NadeKind kind) {
            const bool molotovFamily = (kind == NadeKind::Molotov || kind == NadeKind::Incendiary);
            Vec3 newVel = ClipVel(vel, hit_normal, 2.0f) * kNadeElasticity;
            const float speedSq = newVel.x * newVel.x + newVel.y * newVel.y + newVel.z * newVel.z;
            if (hit_normal.z >= kNadeSteepNormalZ && speedSq > kNadeSteepSpeedSq) {
                const float len = sqrtf(speedSq);
                if (len > 0.0001f) {
                    const float dot = newVel.dot(hit_normal) / len;
                    if (dot > 0.5f) newVel = newVel * (1.5f - dot);
                }
            }
            if (molotovFamily && hit_normal.z >= MolotovSlopeZ()) { vel = { 0,0,0 }; return true; }
            if (speedSq < 400.0f) { vel = { 0,0,0 }; return molotovFamily; }
            vel = newVel;

            const float remaining = 1.0f - frac;
            if (remaining > 0.0f) {
                Vec3 curPos = pos, curVel = newVel;
                float curRem = remaining;
                for (int extra = 0; extra < 2 && curRem > 0.001f; ++extra) {
                    const Vec3 postEnd = curPos + curVel * (curRem * kNadeSimDt);
                    Vec3 h_pos{}, h_norm{}; float h_frac = 1.f;
                    if (!NadeTrace(curPos, postEnd, h_pos, h_norm, h_frac)) { curPos = postEnd; break; }
                    curPos = h_pos;
                    Vec3 postVel = ClipVel(curVel, h_norm, 2.0f) * kNadeElasticity;
                    const float postSq = postVel.x * postVel.x + postVel.y * postVel.y + postVel.z * postVel.z;
                    if (postSq < 400.0f) { curVel = { 0,0,0 }; break; }
                    curVel = postVel;
                    curRem *= (1.0f - h_frac);
                    if (curRem <= 0.001f) break;
                }
                pos = curPos; vel = curVel;
            }
            return false;
        }
        static bool SimulateNade(const Vec3& start, const Vec3& vel, NadeKind kind, NadeResult& out) {
            out.Reset();
            if (kind == NadeKind::None || !IsFinite(start) || !IsFinite(vel)) return false;
            if (!g_trace_resolved) {
                // Fallback: pure parabola hasta tocar la altura de start
                Vec3 p = start, v = vel;
                for (int t = 0; t < 400; ++t) {
                    v.z -= kNadeGravity * kNadeSimDt;
                    p.x += v.x * kNadeSimDt;
                    p.y += v.y * kNadeSimDt;
                    p.z += v.z * kNadeSimDt;
                    if (t % kNadeTicksPerPt == 0 && out.pointCount < NadeResult::kMaxPoints)
                        out.points[out.pointCount++] = p;
                    if (p.z <= start.z - 1.f) break;
                }
                out.endPos = p;
                out.valid = (out.pointCount > 0);
                out.fuse = 1.5f;
                return true;
            }
            Vec3 pos = start, velCur = vel;
            int bounceCount = 0, stride = 0;
            const float molotovFuse = kNadeMolotovFuse + 0.125f;
            const int maxTicks = NadeMaxTicks(kind);
            int tick = 0;
            for (; tick < maxTicks; ++tick) {
                if (stride == 0 && out.pointCount < NadeResult::kMaxPoints)
                    out.points[out.pointCount++] = pos;
                const float newVz = velCur.z - kNadeGravity * kNadeSimDt;
                const Vec3 move(velCur.x * kNadeSimDt, velCur.y * kNadeSimDt,
                    (velCur.z + newVz) * 0.5f * kNadeSimDt);
                velCur.z = newVz;
                const Vec3 traceEnd = pos + move;
                Vec3 h_pos{}, h_norm{}; float h_frac = 1.f;
                bool hit = NadeTrace(pos, traceEnd, h_pos, h_norm, h_frac);
                pos = hit ? h_pos : traceEnd;
                bool impactDetonate = false;
                if (hit) {
                    ++bounceCount;
                    if (out.bounceCount < NadeResult::kMaxBounces)
                        out.bounces[out.bounceCount++] = pos;
                    impactDetonate = ResolveNadeCollision(h_pos, h_norm, h_frac, pos, velCur, kind);
                }
                if (impactDetonate || NadeShouldDetonate(kind, velCur, tick, molotovFuse) || bounceCount > kNadeBounceLimit)
                    break;
                if (hit && (velCur.x == 0.0f && velCur.y == 0.0f && velCur.z == 0.0f))
                    break;
                stride = (hit || ++stride >= kNadeTicksPerPt) ? 0 : stride;
            }
            out.endPos = pos;
            out.valid = (out.pointCount > 0);
            out.fuse = (float)tick * kNadeSimDt;
            if (out.pointCount > 0 && out.pointCount < NadeResult::kMaxPoints) {
                if (Dist(out.points[out.pointCount - 1], out.endPos) > 1.0f)
                    out.points[out.pointCount++] = out.endPos;
            }
            return true;
        }
        static NadeKind ClassifyHeld(const char* name) {
            if (!name || !name[0]) return NadeKind::None;
            if (std::strstr(name, "hegrenade")) return NadeKind::HE;
            if (std::strstr(name, "smokegrenade")) return NadeKind::Smoke;
            if (std::strstr(name, "flashbang")) return NadeKind::Flash;
            if (std::strstr(name, "molotov")) return NadeKind::Molotov;
            if (std::strstr(name, "incgrenade") || std::strstr(name, "incendiary")) return NadeKind::Incendiary;
            if (std::strstr(name, "decoy")) return NadeKind::Decoy;
            return NadeKind::None;
        }
        static NadeKind ClassifyProjectile(const char* d) {
            if (!d || !d[0]) return NadeKind::None;
            if (std::strstr(d, "hegrenade")) return NadeKind::HE;
            if (std::strstr(d, "flashbang")) return NadeKind::Flash;
            if (std::strstr(d, "smokegrenade")) return NadeKind::Smoke;
            if (std::strstr(d, "molotov") || std::strstr(d, "incgrenade")) return NadeKind::Molotov;
            if (std::strstr(d, "decoy")) return NadeKind::Decoy;
            return NadeKind::None;
        }
        static const char* NadeLabel(NadeKind k) {
            switch (k) {
            case NadeKind::Flash: return "FLASHBANG";
            case NadeKind::HE: return "HE GRENADE";
            case NadeKind::Smoke: return "SMOKE";
            case NadeKind::Molotov: return "MOLOTOV";
            case NadeKind::Incendiary: return "INCENDIARY";
            case NadeKind::Decoy: return "DECOY";
            default: return "";
            }
        }
        static float NadeEffectFull(NadeKind k) {
            switch (k) {
            case NadeKind::Smoke: return 18.f;
            case NadeKind::Decoy: return 15.f;
            case NadeKind::Molotov:
            case NadeKind::Incendiary: return 7.f;
            default: return 0.f;
            }
        }

        struct NadeCache {
            float pitch = 0.f, yaw = 0.f;
            Vec3 eye{}, absVel{};
            float strength = -1.f;
            NadeKind kind = NadeKind::None;
            NadeResult res;
            int idleFrames = 0;
        };
        static NadeCache g_nadeCache;
        static ULONGLONG g_lastHeldSimMs = 0;

        static bool NadeInputsChanged(const NadeCache& c) {
            if (c.kind != g_nadeCache.kind) return true;
            if (fabsf(c.strength - g_nadeCache.strength) > 0.005f) return true;
            if (fabsf(c.pitch - g_nadeCache.pitch) > 0.05f || fabsf(c.yaw - g_nadeCache.yaw) > 0.05f) return true;
            if (Dist(c.eye, g_nadeCache.eye) > 0.5f) return true;
            if (Dist(c.absVel, g_nadeCache.absVel) > 5.0f) return true;
            return false;
        }
        static Vec3 ComputeNadeThrowVel(float pitch, float yaw, float strength, const Vec3& inheritVel) {
            float s = strength;
            if (fabsf(s - 0.5f) <= 0.1f) s = 0.5f;
            if (!std::isfinite(s)) s = 1.0f;
            s = std::clamp(s, 0.0f, 1.0f);
            while (pitch > 90.0f) pitch -= 360.0f;
            while (pitch < -90.0f) pitch += 360.0f;
            pitch -= (90.0f - fabsf(pitch)) * 10.f / 90.f;
            const float yawRad = yaw * 3.14159265358979323846f / 180.f;
            const float pitchRad = pitch * 3.14159265358979323846f / 180.f;
            const float cp = cosf(pitchRad);
            Vec3 fwd{ cp * cosf(yawRad), cp * sinf(yawRad), -sinf(pitchRad) };
            float base = 15.0f;
            const float scaled = 750.0f * 0.9f;
            if (scaled >= 15.0f) base = (std::min)(750.0f, scaled);
            const float speed = (s * 0.7f + 0.3f) * base;
            return fwd * speed + inheritVel * kNadeInheritScale;
        }
        static void NadePushOutOrigin(Vec3& origin, const Vec3& dir) {
            const Vec3 candidate{
                origin.x + dir.x * kNadePushoutDist,
                origin.y + dir.y * kNadePushoutDist,
                origin.z + dir.z * kNadePushoutDist,
            };
            Vec3 h_pos{}, h_norm{}; float h_frac = 1.f;
            if (NadeTrace(origin, candidate, h_pos, h_norm, h_frac)) {
                if (IsFinite(h_pos)) { origin = h_pos; return; }
            }
            if (IsFinite(candidate)) origin = candidate;
        }
        static void UpdateNadeCache(uintptr_t local_pawn, uintptr_t weap, uintptr_t client) {
            ULONGLONG now = GetTickCount64();
            if (now - g_lastHeldSimMs < kHeldSimIntervalMs) return;
            g_lastHeldSimMs = now;

            NadeCache next{};
            char designer[64]{}; DesignerName(weap, designer, sizeof(designer));
            next.kind = ClassifyHeld(designer);
            if (next.kind == NadeKind::None) { g_nadeCache = NadeCache{}; return; }

            next.pitch = SafeRead<float>(client + Offsets::dwViewAngles);
            next.yaw = SafeRead<float>(client + Offsets::dwViewAngles + 4);

            bool stashed = SafeRead<bool>(local_pawn + Schema::m_bGrenadeParametersStashed);
            if (stashed) {
                Vec3 stash = SafeRead<Vec3>(local_pawn + Schema::m_vecStashedGrenadeThrowPosition);
                if (IsFinite(stash)) next.eye = stash;
            }
            if (!IsFinite(next.eye)) {
                next.eye = SafeRead<Vec3>(local_pawn + Schema::m_vOldOrigin);
                next.eye.z += 64.f;
            }
            if (!IsFinite(next.eye)) return;

            next.absVel = SafeRead<Vec3>(local_pawn + Schema::m_vecAbsVelocity);
            next.strength = std::clamp(SafeRead<float>(weap + Schema::m_flThrowStrength), 0.f, 1.f);

            float s = next.strength;
            if (fabsf(s - 0.5f) <= 0.1f) s = 0.5f;
            next.eye.z += (s * 12.f - 12.f);

            float pitch = next.pitch;
            while (pitch > 90.0f) pitch -= 360.0f;
            while (pitch < -90.0f) pitch += 360.0f;
            pitch -= (90.0f - fabsf(pitch)) * 10.f / 90.f;
            const float yawRad = next.yaw * 3.14159265358979323846f / 180.f;
            const float pitchRad = pitch * 3.14159265358979323846f / 180.f;
            const float cp = cosf(pitchRad);
            Vec3 dir{ cp * cosf(yawRad), cp * sinf(yawRad), -sinf(pitchRad) };
            NadePushOutOrigin(next.eye, dir);

            if (NadeInputsChanged(next) || g_nadeCache.idleFrames >= kNadeForceResim) {
                g_nadeCache = next;
                g_nadeCache.idleFrames = 0;
                Vec3 v0 = ComputeNadeThrowVel(g_nadeCache.pitch, g_nadeCache.yaw, g_nadeCache.strength, g_nadeCache.absVel);
                SimulateNade(g_nadeCache.eye, v0, g_nadeCache.kind, g_nadeCache.res);
            }
            else {
                ++g_nadeCache.idleFrames;
            }
        }

        static void DrawNadeResult(ImDrawList* dl, const Mat4& vm, const NadeResult& res,
            float alpha, int W, int H, bool minimal) {
            if (!res.valid || res.pointCount < 2) return;
            const ImU32 colMain = IM_COL32(255, 255, 255, (int)(alpha * 255));
            const ImU32 colShadow = IM_COL32(0, 0, 0, (int)(110 * alpha));
            const ImU32 colDotFill = IM_COL32(12, 14, 18, (int)(200 * alpha));

            bool havePrev = false;
            ImVec2 prev{};
            for (int i = 0; i < res.pointCount; ++i) {
                Vec2 cur{};
                if (!ToScreen(res.points[i], cur, vm, W, H)) { havePrev = false; continue; }
                if (havePrev) {
                    dl->AddLine(prev, ImVec2(cur.x, cur.y), colShadow, 3.4f);
                    dl->AddLine(prev, ImVec2(cur.x, cur.y), colMain, 2.0f);
                }
                prev = ImVec2(cur.x, cur.y);
                havePrev = true;
            }

            if (!minimal) {
                const ImU32 colBounce = IM_COL32(255, 255, 255, (int)(0.85f * alpha * 255));
                for (int i = 0; i < res.bounceCount; ++i) {
                    Vec2 s{}; if (!ToScreen(res.bounces[i], s, vm, W, H)) continue;
                    const ImVec2 p{ s.x, s.y };
                    dl->AddCircleFilled(p, 3.6f, colShadow, 12);
                    dl->AddCircleFilled(p, 2.6f, colDotFill, 12);
                    dl->AddCircle(p, 2.6f, colBounce, 12, 1.1f);
                }
            }

            Vec2 se{};
            if (ToScreen(res.endPos, se, vm, W, H)) {
                const ImVec2 p{ se.x, se.y };
                dl->AddCircleFilled(p, 7.5f, colShadow, 24);
                dl->AddCircleFilled(p, 6.5f, colDotFill, 24);
                dl->AddCircle(p, 6.5f, colMain, 24, 1.5f);
                dl->AddCircleFilled(p, 1.8f, colMain, 10);
                constexpr float t = 11.f, g = 3.5f;
                dl->AddLine(ImVec2(p.x - t, p.y), ImVec2(p.x - g, p.y), colMain, 1.3f);
                dl->AddLine(ImVec2(p.x + g, p.y), ImVec2(p.x + t, p.y), colMain, 1.3f);
                dl->AddLine(ImVec2(p.x, p.y - t), ImVec2(p.x, p.y - g), colMain, 1.3f);
                dl->AddLine(ImVec2(p.x, p.y + g), ImVec2(p.x, p.y + t), colMain, 1.3f);
            }
        }
        static void DrawNadeLabel(ImDrawList* dl, const Mat4& vm, const Vec3& worldPos,
            const Vec3& localPos, NadeKind kind, float fuseSec, int W, int H) {
            Vec2 sp{}; if (!ToScreen(worldPos, sp, vm, W, H)) return;
            char buf[64]{};
            if (fuseSec >= 0.f) std::snprintf(buf, sizeof(buf), "%s  %.0fm  T-%.1fs", NadeLabel(kind), (worldPos - localPos).length() * 0.0254f, fuseSec);
            else std::snprintf(buf, sizeof(buf), "%s  %.0fm", NadeLabel(kind), (worldPos - localPos).length() * 0.0254f);
            const ImVec2 tsz = ImGui::CalcTextSize(buf);
            const ImVec2 tp{ sp.x - tsz.x * 0.5f, sp.y - tsz.y - 12.f };
            dl->AddRectFilled(ImVec2(tp.x - 4.f, tp.y - 2.f), ImVec2(tp.x + tsz.x + 4.f, tp.y + tsz.y + 2.f), IM_COL32(10, 12, 16, 170), 3.f);
            dl->AddText(tp, IM_COL32(232, 238, 246, 240), buf);
        }

        struct InAirEntry {
            int idx = 0;
            NadeKind kind = NadeKind::None;
            Vec3 pos{}, vel{}, lastPos{};
            NadeResult res;
            ULONGLONG firstSeenMs = 0, lastSimMs = 0, posMs = 0, restMs = 0;
            bool havePos = false;
        };
        static constexpr int kMaxInAir = 16;
        static InAirEntry g_inAir[kMaxInAir];
        static int g_inAirN = 0;
        static ULONGLONG g_lastAirScanMs = 0;

        static void DrawNadeInAir(ImDrawList* dl, const Mat4& vm, uintptr_t local_pawn,
            uintptr_t client, int W, int H) {
            uintptr_t es = SafeRead<uintptr_t>(client + Offsets::dwEntityList);
            if (!ValidPtr(es)) { g_inAirN = 0; return; }

            ULONGLONG now = GetTickCount64();
            Vec3 local_origin = SafeRead<Vec3>(local_pawn + Schema::m_vOldOrigin);

            if (now - g_lastAirScanMs >= kInAirScanMs) {
                g_lastAirScanMs = now;
                int max_index = SafeRead<int>(es + 0x2120);
                if (max_index <= 0 || max_index > 1024) max_index = 512;

                InAirEntry next[kMaxInAir]; int nextN = 0;
                for (int i = 1; i <= max_index && nextN < kMaxInAir; ++i) {
                    uintptr_t chunk = SafeRead<uintptr_t>(es + 0x10 + (i >> 9) * 8);
                    if (!ValidPtr(chunk)) continue;
                    uintptr_t ent = SafeRead<uintptr_t>(chunk + (i & 0x1FF) * 0x70);
                    if (!ValidPtr(ent)) continue;

                    char d[64]{}; DesignerName(ent, d, sizeof(d));
                    NadeKind kind = ClassifyProjectile(d);
                    if (kind == NadeKind::None) {
                        if (std::strstr(d, "inferno")) kind = NadeKind::Molotov;
                        else continue;
                    }

                    uintptr_t node = SafeRead<uintptr_t>(ent + Schema::m_pGameSceneNode);
                    if (!ValidPtr(node)) continue;
                    if (SafeRead<bool>(node + Schema::m_bDormant)) continue; // skip ghost entities

                    Vec3 pos = SafeRead<Vec3>(node + Schema::m_vecAbsOrigin);
                    if (!IsFinite(pos)) continue;

                    Vec3 netVel = SafeRead<Vec3>(ent + Schema::m_vecAbsVelocity);
                    Vec3 simVel = netVel;

                    const InAirEntry* prior = nullptr;
                    for (int j = 0; j < g_inAirN; ++j) {
                        if (g_inAir[j].idx != i) continue;
                        if (g_inAir[j].havePos && Dist(pos, g_inAir[j].lastPos) < 1000.f) prior = &g_inAir[j];
                        break;
                    }

                    ULONGLONG restMs = prior ? prior->restMs : 0;

                    // Velocity fallback: derive from position delta for remote projectiles
                    if (prior && prior->havePos) {
                        const ULONGLONG dtMs = (now >= prior->posMs) ? (now - prior->posMs) : 0;
                        const float dLen = Dist(pos, prior->lastPos);
                        if (dLen >= 0.75f && netVel.length() < 1.f && dtMs > 0 && dtMs <= 250) {
                            const float invDt = 1000.f / (float)dtMs;
                            simVel = (pos - prior->lastPos) * invDt;
                        }
                        else if (dLen < 0.1f && dtMs >= 150 && restMs == 0) {
                            restMs = prior->posMs;
                        }
                    }
                    const bool resting = restMs != 0 && (now - restMs) >= 150;

                    NadeResult res;
                    const ULONGLONG firstSeen = (prior && prior->firstSeenMs) ? prior->firstSeenMs : now;
                    const bool expired = (now - firstSeen) > kInAirMaxLifeMs;
                    if (!resting && !expired && IsFinite(simVel)) {
                        if (prior && now - prior->lastSimMs < kInAirSimMs) {
                            res = prior->res;
                        }
                        else {
                            SimulateNade(pos, simVel, kind, res);
                        }
                    }

                    next[nextN].idx = i;
                    next[nextN].kind = kind;
                    next[nextN].pos = pos;
                    next[nextN].vel = simVel;
                    next[nextN].lastPos = pos;
                    next[nextN].posMs = now;
                    next[nextN].havePos = true;
                    next[nextN].lastSimMs = now;
                    next[nextN].firstSeenMs = firstSeen;
                    next[nextN].restMs = resting ? restMs : (prior ? prior->restMs : 0);
                    next[nextN].res = res;
                    ++nextN;
                }
                memcpy(g_inAir, next, sizeof(InAirEntry) * nextN);
                g_inAirN = nextN;
            }

            for (int i = 0; i < g_inAirN; ++i) {
                const InAirEntry& e = g_inAir[i];
                const bool resting = e.restMs != 0 && (now - e.restMs) >= 150;
                float fuse = -1.f;
                if (resting) {
                    if (NadeEffectFull(e.kind) > 0.f)
                        fuse = std::max(0.f, NadeEffectFull(e.kind) - (float)(now - e.restMs) / 1000.f);
                    else
                        fuse = std::max(0.f, kHeFlashFuseTotal - (float)(now - e.firstSeenMs) / 1000.f);
                }
                else if (e.kind == NadeKind::HE || e.kind == NadeKind::Flash) {
                    fuse = std::max(0.f, kHeFlashFuseTotal - (float)(now - e.firstSeenMs) / 1000.f);
                }

                if (!resting && e.res.valid)
                    DrawNadeResult(dl, vm, e.res, 0.85f, W, H, false);

                DrawNadeLabel(dl, vm, e.pos, local_origin, e.kind, fuse, W, H);
            }
        }

        static void DrawGrenadePrediction(ImDrawList* dl, int W, int H, const Mat4& vm,
            uintptr_t local_pawn, uintptr_t client) {
            if (!g_cfg.visuals.world.grenade_pred) return;
            if (!ValidPtr(local_pawn)) return;
            ResolveTrace(client);

            uintptr_t ws = SafeRead<uintptr_t>(local_pawn + Schema::m_pWeaponServices);
            if (ValidPtr(ws)) {
                uint32_t h_w = SafeRead<uint32_t>(ws + Schema::m_hActiveWeapon);
                if (h_w && h_w != 0xFFFFFFFF) {
                    uintptr_t es = SafeRead<uintptr_t>(client + Offsets::dwEntityList);
                    if (ValidPtr(es)) {
                        uintptr_t wc = SafeRead<uintptr_t>(es + 0x10 + ((h_w & 0x7FFF) >> 9) * 8);
                        if (ValidPtr(wc)) {
                            uintptr_t weap = SafeRead<uintptr_t>(wc + ((h_w & 0x1FF) * 0x70));
                            if (ValidPtr(weap)) {
                                UpdateNadeCache(local_pawn, weap, client);
                                if (g_nadeCache.kind != NadeKind::None && g_nadeCache.res.valid) {
                                    DrawNadeResult(dl, vm, g_nadeCache.res, 1.0f, W, H, false);
                                    Vec2 land;
                                    if (ToScreen(g_nadeCache.res.endPos, land, vm, W, H)) {
                                        Vec3 origin = SafeRead<Vec3>(local_pawn + Schema::m_vOldOrigin);
                                        float dist = Dist(origin, g_nadeCache.res.endPos) * 0.0254f;
                                        char buf[64];
                                        std::snprintf(buf, sizeof(buf), "%s  %.1fm  %.2fs",
                                            NadeLabel(g_nadeCache.kind), dist, g_nadeCache.res.fuse);
                                        ImVec2 sz = ImGui::CalcTextSize(buf);
                                        dl->AddText(ImVec2(land.x - sz.x * 0.5f, land.y + 14.f),
                                            IM_COL32(240, 240, 240, 230), buf);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            DrawNadeInAir(dl, vm, local_pawn, client, W, H);
        }

        // ═══════════════════════════════════════════════════════════════════
        //  GrenadeHelper — lineups + HUD (sin framework externo)
        // ═══════════════════════════════════════════════════════════════════
        namespace GrenadeHelperSimple {
            enum class ThrowType : int {
                Stand = 0, Jump = 1, Walk = 2, Run = 3, Crouch = 4, RunJump = 5,
                CrouchJump = 6, CrouchWalk = 7, WalkJump = 8, LeftJump = 9,
                RightJump = 10, BackJump = 11, Left = 12, Right = 13, Back = 14
            };
            enum class GHKind : int { Any = 0, HE, Flash, Smoke, Molly, Decoy };

            struct Lineup {
                std::string name, map;
                GHKind kind = GHKind::Any;
                ThrowType throwType = ThrowType::Stand;
                Vec3 pos{};
                float angX = 0.f, angY = 0.f, eyeLift = 64.f;
                bool enabled = true;
            };

            constexpr float kUnitsToM = 0.0254f;
            constexpr float kAimDist = 350.f;
            constexpr float kPadRadius = 14.f;
            constexpr float kAimLockR = 4.5f;
            constexpr float kEyeLiftDef = 64.f;
            constexpr ULONGLONG kGrace = 200;
            constexpr ULONGLONG kMotion = 550;
            constexpr ULONGLONG kArmIgnore = 280;
            constexpr ULONGLONG kTimeout = 20000;

            static std::vector<Lineup> g_lineups;
            static bool g_loaded = false;
            static bool g_prevKeyDown = false;
            static char g_map[128] = {};
            static uintptr_t g_mapClient = 0;

            struct CapState {
                bool armed = false;
                Vec3 pos{};
                float angX = 0.f, angY = 0.f, eyeLift = 64.f;
                GHKind kind = GHKind::Any;
                char name[64]{};
                ULONGLONG armMs = 0;
                bool attackHeld = false;
                bool sawAttack = false;
                ULONGLONG lastJump = 0, lastRun = 0, lastWalk = 0, lastDuck = 0;
                ULONGLONG lastLeft = 0, lastRight = 0, lastBack = 0;
                ULONGLONG attackMs = 0;
                float throwBaseline = 0.f;
                bool thrown = false;
                ULONGLONG pinRelease = 0;
            };
            static CapState g_cap;

            static bool IsFiniteVec3(const Vec3& v) {
                return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
            }
            static void AngleVectors(float pitch, float yaw, Vec3& fwd) {
                const float pr = pitch * 0.017453292f;
                const float yr = yaw * 0.017453292f;
                const float cp = std::cos(pr), sp = std::sin(pr);
                const float cy = std::cos(yr), sy = std::sin(yr);
                fwd = { cp * cy, cp * sy, -sp };
            }
            static Vec3 AimWorldFromStand(const Vec3& stand, float ax, float ay, float lift, float dist) {
                Vec3 fwd{}; AngleVectors(ax, ay, fwd);
                return { stand.x + fwd.x * dist, stand.y + fwd.y * dist, stand.z + lift + fwd.z * dist };
            }
            static bool RecentMs(ULONGLONG t, ULONGLONG now, ULONGLONG win) {
                return t != 0 && now >= t && (now - t) <= win;
            }
            static GHKind ClassifyDesigner(const char* dn) {
                if (!dn || !dn[0]) return GHKind::Any;
                if (std::strstr(dn, "hegrenade")) return GHKind::HE;
                if (std::strstr(dn, "flashbang")) return GHKind::Flash;
                if (std::strstr(dn, "smokegrenade")) return GHKind::Smoke;
                if (std::strstr(dn, "molotov") || std::strstr(dn, "incgrenade")) return GHKind::Molly;
                if (std::strstr(dn, "decoy")) return GHKind::Decoy;
                return GHKind::Any;
            }
            static uintptr_t GetActiveWeapon(uintptr_t local, uintptr_t client) {
                if (!ValidPtr(local) || !ValidPtr(client)) return 0;
                uintptr_t ws = SafeRead<uintptr_t>(local + Schema::m_pWeaponServices);
                if (!ValidPtr(ws)) return 0;
                uint32_t h = SafeRead<uint32_t>(ws + Schema::m_hActiveWeapon);
                if (!h || h == 0xFFFFFFFF) return 0;
                uintptr_t es = SafeRead<uintptr_t>(client + Offsets::dwEntityList);
                if (!ValidPtr(es)) return 0;
                uintptr_t wc = SafeRead<uintptr_t>(es + 0x10 + ((h & 0x7FFF) >> 9) * 8);
                if (!ValidPtr(wc)) return 0;
                uintptr_t weap = SafeRead<uintptr_t>(wc + ((h & 0x1FF) * 0x70));
                return ValidPtr(weap) ? weap : 0;
            }
            static bool GetViewAngles(uintptr_t client, float& pitch, float& yaw) {
                if (!ValidPtr(client)) return false;
                pitch = SafeRead<float>(client + Offsets::dwViewAngles);
                yaw = SafeRead<float>(client + Offsets::dwViewAngles + 4);
                return std::isfinite(pitch) && std::isfinite(yaw);
            }
            static void SyncMap(uintptr_t client) {
                if (g_map[0] && g_mapClient == client) return;
                g_map[0] = 0;
                if (!ValidPtr(client)) return;
                uintptr_t gv = SafeRead<uintptr_t>(client + Offsets::dwGlobalVars);
                if (!ValidPtr(gv)) return;
                uintptr_t p = SafeRead<uintptr_t>(gv + Offsets::globalvars_mapname);
                if (!ValidPtr(p)) return;
                char tmp[128]{};
                if (!SafeMemReadBuf(p, tmp, sizeof(tmp) - 1)) return;
                const char* base = tmp;
                for (const char* q = tmp; *q; ++q) if (*q == '/' || *q == '\\') base = q + 1;
                std::snprintf(g_map, sizeof(g_map), "%s", base);
                for (char* q = g_map; *q; ++q) if (*q == '.') { *q = 0; break; }
                for (char* q = g_map; *q; ++q) *q = (char)std::tolower((unsigned char)*q);
                g_mapClient = client;
            }
            static std::filesystem::path Folder() {
                std::filesystem::path f;
                char* up = nullptr; size_t len = 0;
                if (_dupenv_s(&up, &len, "USERPROFILE") == 0 && up && len > 0) {
                    f = up; free(up);
                    f /= "Documents"; f /= "VantaRage"; f /= "GrenadeHelpers";
                }
                else { f = "VantaRage"; f /= "GrenadeHelpers"; }
                std::error_code ec; std::filesystem::create_directories(f, ec);
                return f;
            }
            static std::filesystem::path File() { return Folder() / "lineups.txt"; }
            static void EscapeField(const std::string& s, std::string& out) {
                out.clear();
                for (char c : s) {
                    if (c == '|' || c == '\\' || c == '\n') { out += '\\'; out += c; }
                    else out += c;
                }
            }
            static std::string UnescapeField(const std::string& s) {
                std::string out; out.reserve(s.size());
                for (size_t i = 0; i < s.size(); ++i) {
                    if (s[i] == '\\' && i + 1 < s.size()) out += s[++i];
                    else out += s[i];
                }
                return out;
            }
            static bool Save() {
                std::ofstream o(File());
                if (!o) return false;
                o << "# name|map|kind|throw|x y z|ax ay lift|enabled\n";
                for (const auto& L : g_lineups) {
                    std::string n, m;
                    EscapeField(L.name, n); EscapeField(L.map, m);
                    o << n << '|' << m << '|' << (int)L.kind << '|' << (int)L.throwType << '|'
                        << L.pos.x << ' ' << L.pos.y << ' ' << L.pos.z << '|'
                        << L.angX << ' ' << L.angY << ' ' << L.eyeLift << '|' << (L.enabled ? 1 : 0) << '\n';
                }
                return true;
            }
            static bool Load() {
                g_lineups.clear(); g_loaded = true;
                std::ifstream in(File());
                if (!in) return true;
                std::string line;
                while (std::getline(in, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    std::string f[7]; int fi = 0; std::string cur;
                    for (size_t i = 0; i < line.size() && fi < 7; ++i) {
                        char c = line[i];
                        if (c == '\\' && i + 1 < line.size()) { cur += c; cur += line[++i]; }
                        else if (c == '|') { f[fi++] = cur; cur.clear(); }
                        else cur += c;
                    }
                    if (fi < 7) f[fi++] = cur;
                    if (fi < 7) continue;
                    Lineup L;
                    L.name = UnescapeField(f[0]);
                    L.map = UnescapeField(f[1]);
                    L.kind = (GHKind)std::clamp(std::atoi(f[2].c_str()), 0, 5);
                    L.throwType = (ThrowType)std::clamp(std::atoi(f[3].c_str()), 0, 14);
                    std::sscanf(f[4].c_str(), "%f %f %f", &L.pos.x, &L.pos.y, &L.pos.z);
                    std::sscanf(f[5].c_str(), "%f %f %f", &L.angX, &L.angY, &L.eyeLift);
                    L.enabled = (f[6] == "1");
                    if (IsFiniteVec3(L.pos)) g_lineups.push_back(std::move(L));
                }
                return true;
            }
            static const char* KindName(GHKind k) {
                switch (k) {
                case GHKind::HE: return "HE"; case GHKind::Flash: return "Flash";
                case GHKind::Smoke: return "Smoke"; case GHKind::Molly: return "Molly";
                case GHKind::Decoy: return "Decoy"; default: return "Any";
                }
            }
            static const char* ThrowName(ThrowType t) {
                switch (t) {
                case ThrowType::Jump: return "Jump"; case ThrowType::Walk: return "Walk";
                case ThrowType::Run: return "Run";   case ThrowType::Crouch: return "Crouch";
                case ThrowType::RunJump: return "Run+Jump";
                case ThrowType::CrouchJump: return "Crouch+Jump";
                case ThrowType::CrouchWalk: return "Crouch+Walk";
                case ThrowType::WalkJump: return "Walk+Jump";
                case ThrowType::LeftJump: return "Left+Jump";
                case ThrowType::RightJump: return "Right+Jump";
                case ThrowType::BackJump: return "Back+Jump";
                case ThrowType::Left: return "Left"; case ThrowType::Right: return "Right";
                case ThrowType::Back: return "Back"; default: return "Stand";
                }
            }
            static ThrowType DetectThrow(uintptr_t local, const CapState& cap) {
                if (!ValidPtr(local)) return ThrowType::Stand;
                uint32_t flags = SafeRead<uint32_t>(local + Schema::m_fFlags);
                Vec3 vel = SafeRead<Vec3>(local + Schema::m_vecAbsVelocity);
                const ULONGLONG now = GetTickCount64();
                const float spd2d = (std::isfinite(vel.x) && std::isfinite(vel.y))
                    ? std::sqrt(vel.x * vel.x + vel.y * vel.y) : 0.f;
                const bool onGround = (flags & 0x1) != 0;
                const bool duckingNow = (flags & 0x4) != 0 || (flags & 0x40) != 0;
                const bool sp = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                const bool sh = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
                const bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
                const bool d = (GetAsyncKeyState('D') & 0x8000) != 0;
                const bool s = (GetAsyncKeyState('S') & 0x8000) != 0;
                const bool w = (GetAsyncKeyState('W') & 0x8000) != 0;
                const bool rising = std::isfinite(vel.z) && vel.z > 40.f;
                const bool air = !onGround && std::isfinite(vel.z) && std::fabs(vel.z) > 20.f;
                const bool hadPin = cap.sawAttack;
                const bool jump = hadPin && (RecentMs(cap.lastJump, now, kMotion) || sp || rising || air);
                const bool duck = (hadPin && RecentMs(cap.lastDuck, now, kMotion)) || duckingNow;
                const bool run = hadPin && (RecentMs(cap.lastRun, now, kMotion) || (spd2d > 135.f && !sh && onGround) || (w && !sh && spd2d > 25.f));
                const bool walk = hadPin && (RecentMs(cap.lastWalk, now, kMotion) || sh || (spd2d > 80.f && spd2d <= 135.f && onGround));
                const bool mLeft = hadPin && (RecentMs(cap.lastLeft, now, kMotion) || a);
                const bool mRight = hadPin && (RecentMs(cap.lastRight, now, kMotion) || d);
                const bool mBack = hadPin && (RecentMs(cap.lastBack, now, kMotion) || s);
                if (duck) {
                    if (jump) return ThrowType::CrouchJump;
                    if (run || walk || spd2d > 20.f) return ThrowType::CrouchWalk;
                    return ThrowType::Crouch;
                }
                if (jump) {
                    if (mLeft && !run) return ThrowType::LeftJump;
                    if (mRight && !run) return ThrowType::RightJump;
                    if (mBack && !run) return ThrowType::BackJump;
                    if (run) return ThrowType::RunJump;
                    if (walk) return ThrowType::WalkJump;
                    return ThrowType::Jump;
                }
                if (mLeft && !run) return ThrowType::Left;
                if (mRight && !run) return ThrowType::Right;
                if (mBack && !run) return ThrowType::Back;
                if (run || (hadPin && spd2d > 135.f)) return ThrowType::Run;
                if (walk) return ThrowType::Walk;
                return ThrowType::Stand;
            }
            static void SampleMotion(uintptr_t local, uintptr_t client) {
                if (!ValidPtr(local) || !g_cap.armed) return;
                const bool released = g_cap.thrown || g_cap.pinRelease != 0;
                float px = 0.f, py = 0.f;
                if (!released && GetViewAngles(client, px, py)) {
                    g_cap.angX = px; g_cap.angY = py;
                }
                uint32_t flags = SafeRead<uint32_t>(local + Schema::m_fFlags);
                Vec3 vel = SafeRead<Vec3>(local + Schema::m_vecAbsVelocity);
                const ULONGLONG now = GetTickCount64();
                const float spd2d = (std::isfinite(vel.x) && std::isfinite(vel.y))
                    ? std::sqrt(vel.x * vel.x + vel.y * vel.y) : 0.f;
                const bool onGround = (flags & 0x1) != 0;
                // Fix 1: eyeLift siempre refresca en onGround
                if (!released && onGround) g_cap.eyeLift = kEyeLiftDef;
                // Posición sí requiere quietud
                if (!released && spd2d < 5.f && onGround) {
                    Vec3 o = SafeRead<Vec3>(local + Schema::m_vOldOrigin);
                    if (IsFiniteVec3(o)) g_cap.pos = o;
                }
                const bool sp = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                const bool w = (GetAsyncKeyState('W') & 0x8000) != 0;
                const bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
                const bool d = (GetAsyncKeyState('D') & 0x8000) != 0;
                const bool s = (GetAsyncKeyState('S') & 0x8000) != 0;
                const bool sh = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
                const bool rising = std::isfinite(vel.z) && vel.z > 40.f;
                const bool air = !onGround && std::isfinite(vel.z) && std::fabs(vel.z) > 20.f;
                const bool ducking = (flags & 0x4) != 0 || (flags & 0x40) != 0;
                if (g_cap.sawAttack) {
                    if (sp || rising || air) g_cap.lastJump = now;
                    if (ducking) g_cap.lastDuck = now;
                    if (sh || (spd2d > 80.f && spd2d <= 135.f)) g_cap.lastWalk = now;
                    if (spd2d > 135.f || (w && !sh)) g_cap.lastRun = now;
                    if (a) g_cap.lastLeft = now;
                    if (d) g_cap.lastRight = now;
                    if (s) g_cap.lastBack = now;
                }
            }
            static bool Arm(const char* name, GHKind kindOverride, uintptr_t local, uintptr_t client) {
                if (!ValidPtr(local)) return false;
                Vec3 pos = SafeRead<Vec3>(local + Schema::m_vOldOrigin);
                if (!IsFiniteVec3(pos)) return false;
                float px = 0.f, py = 0.f;
                if (!GetViewAngles(client, px, py)) return false;
                SyncMap(client);
                g_cap = {};
                g_cap.armed = true;
                g_cap.pos = pos;
                g_cap.angX = px; g_cap.angY = py;
                g_cap.eyeLift = kEyeLiftDef;
                g_cap.armMs = GetTickCount64();
                std::snprintf(g_cap.name, sizeof(g_cap.name), "%s", (name && name[0]) ? name : "Lineup");
                g_cap.kind = kindOverride;
                return true;
            }
            static bool Finish(uintptr_t local, uintptr_t client) {
                if (!g_cap.armed || !ValidPtr(local)) return false;
                SyncMap(client);
                if (!g_map[0]) { g_cap = {}; return false; }
                SampleMotion(local, client);
                Lineup L;
                L.name = g_cap.name[0] ? g_cap.name : "Lineup";
                L.map = g_map;
                L.throwType = DetectThrow(local, g_cap);
                L.pos = g_cap.pos;
                L.angX = g_cap.angX; L.angY = g_cap.angY;
                L.eyeLift = g_cap.eyeLift;
                L.kind = g_cap.kind;
                L.enabled = true;
                g_lineups.push_back(std::move(L));
                Save();
                g_cap = {};
                return true;
            }
            static void Update(uintptr_t client, uintptr_t local) {
                if (!g_cfg.visuals.world.grenade_helper) {
                    if (g_cap.armed) g_cap = {};
                    return;
                }
                if (!g_loaded) Load();
                SyncMap(client);
                const int key = g_cfg.visuals.world.grenade_helper_key;
                const bool keyDown = key && (GetAsyncKeyState(key) & 0x8000) != 0;
                if (keyDown && !g_prevKeyDown) {
                    if (g_cap.armed) g_cap = {};
                    else Arm(g_cfg.visuals.world.grenade_helper_capture_name,
                        (GHKind)std::clamp(g_cfg.visuals.world.grenade_helper_capture_kind, 0, 5),
                        local, client);
                }
                g_prevKeyDown = keyDown;
                if (!g_cap.armed) return;
                const ULONGLONG now = GetTickCount64();
                if (now - g_cap.armMs > kTimeout) { g_cap = {}; return; }
                if (!ValidPtr(local)) { g_cap = {}; return; }
                const bool armGrace = (now - g_cap.armMs) < kArmIgnore;
                const bool wantMouse = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
                const bool atkRaw = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0
                    || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
                uintptr_t wep = GetActiveWeapon(local, client);
                char designer[64]{}; DesignerName(wep, designer, sizeof(designer));
                GHKind held = ClassifyDesigner(designer);
                const bool holdingNade = held != GHKind::Any;
                bool pinPulled = false, heldByPlayer = false;
                float throwTime = 0.f;
                if (holdingNade) {
                    pinPulled = SafeRead<bool>(wep + Schema::m_bPinPulled);
                    heldByPlayer = SafeRead<bool>(wep + Schema::m_bIsHeldByPlayer);
                    throwTime = SafeRead<float>(wep + Schema::m_fThrowTime);
                }
                const bool pinning = holdingNade && !armGrace && !wantMouse
                    && ((pinPulled && heldByPlayer) || atkRaw);
                if (pinning) {
                    g_cap.sawAttack = true;
                    if (!g_cap.attackHeld) {
                        g_cap.attackHeld = true;
                        g_cap.attackMs = now;
                        g_cap.throwBaseline = throwTime;
                        g_cap.thrown = false;
                        g_cap.pinRelease = 0;
                        // Fix 2: re-clasificar kind al primer pin-pull real
                        if (g_cfg.visuals.world.grenade_helper_capture_kind == 0)
                            g_cap.kind = held;
                    }
                }
                const bool throwFresh = throwTime > 0.f && throwTime != g_cap.throwBaseline;
                if (g_cap.attackHeld && throwFresh) g_cap.thrown = true;
                if (g_cap.attackHeld && !g_cap.thrown) {
                    if (!pinPulled && !atkRaw) {
                        if (g_cap.pinRelease == 0) g_cap.pinRelease = now;
                        const ULONGLONG hold = (g_cap.attackMs && now >= g_cap.attackMs) ? (now - g_cap.attackMs) : 0;
                        const bool motionThrow = RecentMs(g_cap.lastJump, now, kMotion) || RecentMs(g_cap.lastRun, now, kMotion);
                        const bool graceOk = g_cap.pinRelease && (now - g_cap.pinRelease) >= kGrace;
                        if (graceOk && g_cap.sawAttack && !wantMouse && (hold >= 70 || motionThrow))
                            g_cap.thrown = true;
                    }
                }
                SampleMotion(local, client);
                if (g_cap.thrown && !wantMouse) Finish(local, client);
            }
            static void Draw(ImDrawList* dl, int W, int H, const Mat4& vm, uintptr_t client, uintptr_t local) {
                if (!g_cfg.visuals.world.grenade_helper || !dl) return;
                const ULONGLONG now = GetTickCount64();

                // Banner de captura
                if (g_cap.armed) {
                    const float leftSec = (g_cap.armMs + kTimeout > now) ? (float)(g_cap.armMs + kTimeout - now) / 1000.f : 0.f;
                    char line[160]{};
                    std::snprintf(line, sizeof(line), "Capture  %s  |  %s  |  throw to save  |  %.0fs",
                        g_cap.name[0] ? g_cap.name : "Lineup", KindName(g_cap.kind), leftSec);
                    ImVec2 ts = ImGui::CalcTextSize(line);
                    ImVec2 p0(W * 0.5f - ts.x * 0.5f - 12.f, 30.f);
                    ImVec2 p1(p0.x + ts.x + 24.f, p0.y + ts.y + 12.f);
                    dl->AddRectFilled(p0, p1, IM_COL32(16, 18, 23, 234), 6.f);
                    dl->AddText(ImVec2(p0.x + 12.f, p0.y + 6.f), IM_COL32(238, 242, 248, 255), line);
                    Vec2 sp;
                    if (ToScreen(g_cap.pos, sp, vm, W, H)) {
                        dl->AddCircleFilled(ImVec2(sp.x, sp.y), 8.f, IM_COL32(120, 190, 255, 220), 24);
                        dl->AddCircle(ImVec2(sp.x, sp.y), 12.f, IM_COL32(120, 190, 255, 120), 24, 1.5f);
                    }
                }

                if (!g_cfg.visuals.world.grenade_helper_capture_name[0] && g_lineups.empty()) return;
                if (g_lineups.empty() || !g_map[0]) return;

                Vec3 origin = SafeRead<Vec3>(local + Schema::m_vOldOrigin);
                if (!IsFiniteVec3(origin)) return;

                GHKind held = GHKind::Any;
                if (g_cfg.visuals.world.grenade_helper_only_held) {
                    uintptr_t wep = GetActiveWeapon(local, client);
                    char d[64]{}; DesignerName(wep, d, sizeof(d));
                    held = ClassifyDesigner(d);
                    if (held == GHKind::Any) return;
                }

                const int selR = std::clamp(g_cfg.visuals.world.grenade_helper_select_dist, 50, 2000);
                const float selR2 = (float)(selR * selR);
                const int standR = std::clamp(g_cfg.visuals.world.grenade_helper_stand_dist, 100, 2500);
                const float standR2 = (float)(standR * standR);

                struct PadRef { int idx; float d2; };
                PadRef padRefs[16];
                int padN = 0;

                for (int i = 0; i < (int)g_lineups.size(); ++i) {
                    const Lineup& L = g_lineups[i];
                    if (!L.enabled) continue;
                    if (L.map.empty() || _stricmp(L.map.c_str(), g_map) != 0) continue;
                    if (g_cfg.visuals.world.grenade_helper_only_held && held != GHKind::Any
                        && L.kind != GHKind::Any && L.kind != held) continue;
                    const float dx = origin.x - L.pos.x, dy = origin.y - L.pos.y, dz = origin.z - L.pos.z;
                    const float d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 > standR2) continue;
                    if (padN < 16) {
                        padRefs[padN++] = { i, d2 };
                    }
                    else {
                        int worst = 0;
                        for (int j = 1; j < 16; ++j)
                            if (padRefs[j].d2 > padRefs[worst].d2) worst = j;
                        if (d2 < padRefs[worst].d2)
                            padRefs[worst] = { i, d2 };
                    }
                }

                for (int i = 0; i < padN; ++i) {
                    int best = i;
                    for (int j = i + 1; j < padN; ++j)
                        if (padRefs[j].d2 < padRefs[best].d2) best = j;
                    if (best != i) {
                        PadRef tmp = padRefs[i];
                        padRefs[i] = padRefs[best];
                        padRefs[best] = tmp;
                    }
                }

                for (int k = 0; k < padN && k < 8; ++k) {
                    const Lineup& L = g_lineups[padRefs[k].idx];
                    const float dist = std::sqrt(padRefs[k].d2);
                    const float alpha = std::clamp(1.f - (dist / (float)standR) * 1.4f, 0.35f, 1.f);
                    const bool inRange = padRefs[k].d2 <= selR2;
                    const float* col = inRange ? g_cfg.visuals.world.grenade_helper_aim_color
                        : g_cfg.visuals.world.grenade_helper_color;
                    const ImU32 ringCol = IM_COL32(
                        (int)(col[0] * 255), (int)(col[1] * 255), (int)(col[2] * 255), (int)(alpha * 200));

                    Vec2 sp;
                    if (!ToScreen(L.pos, sp, vm, W, H)) continue;

                    dl->AddCircle(ImVec2(sp.x, sp.y), kPadRadius, ringCol, 24, inRange ? 2.0f : 1.2f);
                    dl->AddCircleFilled(ImVec2(sp.x, sp.y), 2.5f, ringCol, 12);

                    char lbl[96];
                    std::snprintf(lbl, sizeof(lbl), "%s  [%s]  %.0fm",
                        L.name.c_str(), ThrowName(L.throwType), dist * kUnitsToM);
                    ImVec2 tsz = ImGui::CalcTextSize(lbl);
                    ImVec2 tp(sp.x - tsz.x * 0.5f, sp.y + kPadRadius + 4.f);
                    dl->AddRectFilled(ImVec2(tp.x - 4.f, tp.y - 2.f),
                        ImVec2(tp.x + tsz.x + 4.f, tp.y + tsz.y + 2.f),
                        IM_COL32(10, 12, 16, (int)(alpha * 200)), 3.f);
                    dl->AddText(tp, ringCol, lbl);

                    if (k == 0 || inRange) {
                        Vec3 aimW = AimWorldFromStand(L.pos, L.angX, L.angY, L.eyeLift, kAimDist);
                        Vec2 ap;
                        if (ToScreen(aimW, ap, vm, W, H)) {
                            dl->AddCircleFilled(ImVec2(ap.x, ap.y), 5.f, IM_COL32(0, 0, 0, 140), 16);
                            dl->AddCircle(ImVec2(ap.x, ap.y), 4.5f, ringCol, 16, inRange ? 2.0f : 1.2f);
                            dl->AddCircleFilled(ImVec2(ap.x, ap.y), 1.8f, ringCol, 8);
                            dl->AddLine(ImVec2(sp.x, sp.y), ImVec2(ap.x, ap.y),
                                IM_COL32((int)(col[0] * 255), (int)(col[1] * 255), (int)(col[2] * 255), (int)(alpha * 100)), 1.f);
                        }
                    }
                }
            }
            static void EnsureLoaded() { if (!g_loaded) Load(); }
        } // namespace GrenadeHelperSimple
    } // namespace

    void PushDamage(const Vec3& pos, int dmg) {
        g_dmg.push_back({ pos, (float)ImGui::GetTime(), dmg });
        if (g_dmg.size() > 64) g_dmg.erase(g_dmg.begin());
    }

    void Render() {
        auto& v = g_cfg.visuals.player;
        auto& wld = g_cfg.visuals.world;
        bool any_player = v.esp_box || v.esp_name || v.esp_health || v.esp_armor || v.esp_weapon || v.esp_distance || v.esp_skeleton || v.glow || v.chams;
        bool any_world = wld.radar || wld.bomb_timer || wld.bomb_esp || wld.dropped_weapons || wld.grenade_pred || wld.damage_markers || wld.no_scope || v.glow_weapons || v.glow_bomb;
        bool gh_on = wld.grenade_helper;
        if (!any_player && !any_world && !gh_on) return;

        static uintptr_t client = 0;
        if (!client) client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;
        ImGuiIO& io = ImGui::GetIO();
        int W = (int)io.DisplaySize.x, H = (int)io.DisplaySize.y;

        Mat4 vm = SafeRead<Mat4>(client + Offsets::dwViewMatrix);
        uintptr_t local = SafeRead<uintptr_t>(client + Offsets::dwLocalPlayerPawn);
        uintptr_t es = SafeRead<uintptr_t>(client + Offsets::dwEntityList);

        static EntityList entities;
        entities.Update(local, es);

        Vec3 local_origin{};
        if (ValidPtr(local)) local_origin = SafeRead<Vec3>(local + Schema::m_vOldOrigin);
        int local_team = entities.local_team;
        bool local_scoped = ValidPtr(local) ? SafeRead<bool>(local + Schema::m_bIsScoped) : false;

        static int g_last_dmg = 0;
        if (wld.damage_markers && ValidPtr(local)) {
            uintptr_t ctrl = SafeRead<uintptr_t>(client + Offsets::dwLocalPlayerController);
            if (ValidPtr(ctrl)) {
                uintptr_t ats = SafeRead<uintptr_t>(ctrl + 0x828);
                if (ValidPtr(ats)) {
                    int dealt = SafeRead<int>(ats + Schema::m_flTotalRoundDamageDealt);
                    if (dealt > g_last_dmg && dealt - g_last_dmg < 200) {
                        Vec3 pos = local_origin; pos.z += 60.f;
                        PushDamage(pos, dealt - g_last_dmg);
                    }
                    g_last_dmg = dealt;
                }
            }
        }

        Debug::g_esp.entities_found = (int)entities.players.size();
        Debug::g_esp.entities_on_screen = 0;
        Debug::g_esp.local_x = local_origin.x;
        Debug::g_esp.local_y = local_origin.y;
        Debug::g_esp.local_z = local_origin.z;

        for (const auto& p : entities.players) {
            Vec2 hs, fs;
            if (!ToScreen(p.head, hs, vm, W, H)) continue;
            if (!ToScreen(p.feet, fs, vm, W, H)) continue;
            float top = std::min(hs.y, fs.y), bot = std::max(hs.y, fs.y);
            float h = bot - top;
            if (h < 4.f || h > 2000.f) continue;
            float l, r;
            Vec2 cl, cr;
            if (ToScreen(p.bones[Bones::CLAV_L], cl, vm, W, H) && ToScreen(p.bones[Bones::CLAV_R], cr, vm, W, H)) {
                float mx = (cl.x + cr.x) * 0.5f;
                float bw = std::clamp(std::fabs(cr.x - cl.x) * 1.35f, h * 0.40f, h * 0.70f);
                l = mx - bw * 0.5f; r = mx + bw * 0.5f;
            }
            else { float bw = h * 0.45f; l = hs.x - bw * 0.5f; r = hs.x + bw * 0.5f; }
            if (r < 0.f || l > W || bot < 0.f || top > H) continue;
            Debug::g_esp.entities_on_screen++;
            if (Debug::g_esp.entities_on_screen == 1) {
                Debug::g_esp.target_x = p.origin.x;
                Debug::g_esp.target_y = p.origin.y;
                Debug::g_esp.target_z = p.origin.z;
            }
            bool enemy = (p.team != local_team);
            ImU32 box_col = ToCol(enemy ? v.box_enemy : v.box_team);
            ImU32 name_col = ToCol(v.name_color);
            ImU32 weap_col = ToCol(v.weapon_color);
            ImU32 dist_col = ToCol(v.distance_color);
            if (v.esp_box) {
                dl->AddRect(ImVec2(l - 1.f, top - 1.f), ImVec2(r + 1.f, bot + 1.f), IM_COL32(0, 0, 0, 90), 0.f, 0, 1.f);
                dl->AddRect(ImVec2(l, top), ImVec2(r, bot), box_col, 0.f, 0, 1.25f);
            }
            if (v.esp_health) {
                float f = std::clamp(p.health / 100.f, 0.f, 1.f);
                dl->AddRectFilled(ImVec2(l - 6.f, top), ImVec2(l - 3.f, bot), IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled(ImVec2(l - 6.f, bot - h * f), ImVec2(l - 3.f, bot), HpGradient(p.health));
                if (p.health < 100) {
                    char hbuf[8]; std::snprintf(hbuf, sizeof(hbuf), "%d", p.health);
                    dl->AddText(ImVec2(l - 14.f, bot - h * f - 9.f), HpGradient(p.health), hbuf);
                }
            }
            if (v.esp_armor && p.armor > 0) {
                float f = std::clamp(p.armor / 100.f, 0.f, 1.f);
                dl->AddRectFilled(ImVec2(r + 3.f, top), ImVec2(r + 6.f, bot), IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled(ImVec2(r + 3.f, bot - h * f), ImVec2(r + 6.f, bot), IM_COL32(80, 160, 255, 230));
            }
            if (v.esp_name && p.name[0]) {
                ImVec2 sz = ImGui::CalcTextSize(p.name);
                float tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, top - sz.y - 3.f + 1.f), IM_COL32(0, 0, 0, 180), p.name);
                dl->AddText(ImVec2(tx, top - sz.y - 3.f), name_col, p.name);
            }
            float below = bot + 3.f;
            if (v.esp_weapon && p.weapon[0]) {
                ImVec2 sz = ImGui::CalcTextSize(p.weapon);
                float tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, below + 1.f), IM_COL32(0, 0, 0, 180), p.weapon);
                dl->AddText(ImVec2(tx, below), weap_col, p.weapon);
                below += sz.y + 1.f;
            }
            if (v.esp_distance) {
                char dbuf[24]; std::snprintf(dbuf, sizeof(dbuf), "%.1fm", p.distance * 0.0254f);
                ImVec2 sz = ImGui::CalcTextSize(dbuf);
                float tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, below + 1.f), IM_COL32(0, 0, 0, 180), dbuf);
                dl->AddText(ImVec2(tx, below), dist_col, dbuf);
            }
            if (v.esp_skeleton) {
                ImU32 sk = IM_COL32((int)(v.skel_color[0] * 255), (int)(v.skel_color[1] * 255),
                    (int)(v.skel_color[2] * 255), 235);
                for (int li = 0; li < Bones::LINK_COUNT; ++li) {
                    int b0 = Bones::links[li][0], b1 = Bones::links[li][1];
                    if (!IsFinite(p.bones[b0]) || !IsFinite(p.bones[b1])) continue;
                    Vec2 a, b;
                    if (!ToScreen(p.bones[b0], a, vm, W, H)) continue;
                    if (!ToScreen(p.bones[b1], b, vm, W, H)) continue;
                    dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), sk, 1.15f);
                }
            }
        }

        if (wld.radar) DrawRadar(dl, W, entities.players, local_origin, local_team, vm, wld.radar_pos, wld.radar_zoom);
        if (wld.bomb_timer || wld.bomb_esp) DrawBombTimer(dl, W, client);
        if (wld.dropped_weapons || v.glow_weapons) DrawDroppedWeapons(dl, W, H, vm, es, local);
        if (wld.grenade_pred) DrawGrenadePrediction(dl, W, H, vm, local, client);
        if (wld.damage_markers) DrawDamageMarkers(dl, W, H, vm);
        if (wld.no_scope) DrawNoScopeOverlay(dl, W, H, local_scoped);

        if (gh_on && ValidPtr(local)) {
            GrenadeHelperSimple::Update(client, local);
            GrenadeHelperSimple::Draw(dl, W, H, vm, client, local);
        }
    }
}