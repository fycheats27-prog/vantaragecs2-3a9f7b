// --- esp.cpp ---
#include "esp.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include "debug.h"
#include <imgui.h>
#include <windows.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace Features::Visuals::ESP {
    namespace {
        struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };
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
        }
        namespace Offsets {
            constexpr uintptr_t dwEntityList = 0x271A908, dwLocalPlayerPawn = 0x2565718;
            constexpr uintptr_t dwLocalPlayerController = 0x253CB78, dwViewMatrix = 0x256B020;
            constexpr uintptr_t dwViewAngles = 0x257BCC8, dwGlobalVars = 0x2230090;
            constexpr uintptr_t dwPlantedC4 = 0x24CB3C0, dwCSGOInput = 0x257B640;
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
            if (!std::strcmp(s, "fiveseven")) return "Five-SeveN";
            if (!std::strcmp(s, "p250")) return "P250";
            if (!std::strcmp(s, "tec9")) return "Tec-9";
            if (!std::strcmp(s, "cz75a")) return "CZ75";
            if (!std::strcmp(s, "revolver")) return "R8";
            if (!std::strcmp(s, "ak47")) return "AK-47";
            if (!std::strcmp(s, "m4a1")) return "M4A4";
            if (!std::strcmp(s, "m4a1_silencer")) return "M4A1-S";
            if (!std::strcmp(s, "aug")) return "AUG";
            if (!std::strcmp(s, "sg556")) return "SG 553";
            if (!std::strcmp(s, "galilar")) return "Galil";
            if (!std::strcmp(s, "famas")) return "FAMAS";
            if (!std::strcmp(s, "awp")) return "AWP";
            if (!std::strcmp(s, "ssg08")) return "Scout";
            if (!std::strcmp(s, "scar20")) return "SCAR-20";
            if (!std::strcmp(s, "g3sg1")) return "G3SG1";
            if (!std::strcmp(s, "mac10")) return "MAC-10";
            if (!std::strcmp(s, "mp9")) return "MP9";
            if (!std::strcmp(s, "mp7")) return "MP7";
            if (!std::strcmp(s, "mp5sd")) return "MP5-SD";
            if (!std::strcmp(s, "ump45")) return "UMP-45";
            if (!std::strcmp(s, "p90")) return "P90";
            if (!std::strcmp(s, "bizon")) return "Bizon";
            if (!std::strcmp(s, "nova")) return "Nova";
            if (!std::strcmp(s, "xm1014")) return "XM1014";
            if (!std::strcmp(s, "mag7")) return "MAG-7";
            if (!std::strcmp(s, "sawedoff")) return "Sawed-Off";
            if (!std::strcmp(s, "m249")) return "M249";
            if (!std::strcmp(s, "negev")) return "Negev";
            if (!std::strcmp(s, "taser")) return "Zeus";
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
            if (p1 < 0x100000000ULL) {
                uint32_t h = (uint32_t)p1;
                uintptr_t es = SafeRead<uintptr_t>(client + Offsets::dwEntityList);
                if (ValidPtr(es) && h && h != 0xFFFFFFFF) {
                    uintptr_t ch = SafeRead<uintptr_t>(es + 0x10 + 8 * ((h & 0x7FFF) >> 9));
                    if (ValidPtr(ch)) return SafeRead<uintptr_t>(ch + 0x70 * (h & 0x1FF));
                }
            }
            return 0;
        }

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
                dl->AddRect(p0, p1, crit ? IM_COL32(220, 60, 50, 255) : IM_COL32(40, 140, 240, 255), 6.f, 0, 1.4f + pulse * (crit ? 1.5f : 0.f));
                float bw = CW - 16.f, bx = cx - bw * 0.5f, by = cy + 8.f;
                float prog = std::clamp(rem / 40.f, 0.f, 1.f);
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + 7.f), IM_COL32(30, 34, 42, 255), 3.f);
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * prog, by + 7.f), crit ? IM_COL32(230, 55, 45, 255) : IM_COL32(40, 145, 245, 255), 3.f);
                char buf[48]; std::snprintf(buf, sizeof(buf), "C4  %s   %.1fs", site == 1 ? "B" : "A", rem);
                ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(cx - ts.x * 0.5f, by + 12.f), crit ? IM_COL32(255, 90, 80, 255) : IM_COL32(235, 240, 250, 255), buf);
                if (defusing && def_rem > 0.f) {
                    char d[32]; std::snprintf(d, sizeof(d), "defusing  %.1fs", def_rem);
                    ImVec2 ds = ImGui::CalcTextSize(d);
                    dl->AddText(ImVec2(cx - ds.x * 0.5f, by + 28.f), IM_COL32(80, 230, 150, 255), d);
                }
            }
            if (g_cfg.visuals.player.glow_bomb)
                ApplyGlow(c4, g_cfg.visuals.player.glow_bomb_color);
        }

        static void DrawRadar(ImDrawList* dl, int W, const std::vector<Player>& players, const Vec3& local_origin, int local_team, const Mat4& vm, int cfg_pos, int cfg_zoom) {
            if (!IsFinite(local_origin)) return;
            constexpr float BASE = 260.f;
            float zoom = 1.f + cfg_zoom * 0.01f, RANGE = 6500.f / zoom, PADDING = 16.f, RS = BASE;
            float rx = (cfg_pos == 0) ? PADDING : (cfg_pos == 2) ? (W - RS) * 0.5f : W - RS - PADDING;
            float ry = PADDING, cx = rx + RS * 0.5f, cy = ry + RS * 0.5f, scale = (RS * 0.46f) / RANGE;
            float yaw = std::atan2(-vm.m[1][2], -vm.m[0][2]);
            dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + RS, ry + RS), IM_COL32(12, 14, 18, 240), 8.f);
            dl->AddRect(ImVec2(rx, ry), ImVec2(rx + RS, ry + RS), IM_COL32(50, 160, 230, 200), 8.f, 0, 1.8f);
            float dx = std::cos(yaw), dy = -std::sin(yaw);
            dl->AddTriangleFilled(ImVec2(cx + dx * 10.f, cy + dy * 10.f), ImVec2(cx - dy * 6.5f - dx * 5.5f, cy + dx * 6.5f - dy * 5.5f), ImVec2(cx + dy * 6.5f - dx * 5.5f, cy - dx * 6.5f - dy * 5.5f), IM_COL32(100, 235, 255, 255));
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
            if (!is_scoped || !g_cfg.visuals.world.no_scope) return;
            const float bar = H * 0.16f;
            dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)W, bar), IM_COL32(0, 0, 0, 255));
            dl->AddRectFilled(ImVec2(0, H - bar), ImVec2((float)W, (float)H), IM_COL32(0, 0, 0, 255));
            const float cx = W * 0.5f, cy = H * 0.5f;
            const float gap = 3.f, len = 22.f, thick = 1.4f;
            const ImU32 col = IM_COL32(255, 255, 255, 230);
            const ImU32 sh = IM_COL32(0, 0, 0, 160);
            // shadow
            dl->AddLine(ImVec2(cx - len - gap, cy), ImVec2(cx - gap, cy), sh, thick + 1.f);
            dl->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + len + gap, cy), sh, thick + 1.f);
            dl->AddLine(ImVec2(cx, cy - len - gap), ImVec2(cx, cy - gap), sh, thick + 1.f);
            dl->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + len + gap), sh, thick + 1.f);
            // lines
            dl->AddLine(ImVec2(cx - len - gap, cy), ImVec2(cx - gap, cy), col, thick);
            dl->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + len + gap, cy), col, thick);
            dl->AddLine(ImVec2(cx, cy - len - gap), ImVec2(cx, cy - gap), col, thick);
            dl->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + len + gap), col, thick);
            dl->AddCircle(ImVec2(cx, cy), 1.8f, col, 12, 1.f);
        }

        static void DrawGrenadePrediction(ImDrawList* dl, int W, int H, const Mat4& vm, uintptr_t local_pawn, uintptr_t client) {
            if (!g_cfg.visuals.world.grenade_pred || !ValidPtr(local_pawn)) return;

            uintptr_t ws = SafeRead<uintptr_t>(local_pawn + Schema::m_pWeaponServices);
            if (!ValidPtr(ws)) return;
            uint32_t h_w = SafeRead<uint32_t>(ws + Schema::m_hActiveWeapon);
            if (!h_w || h_w == 0xFFFFFFFF) return;

            uintptr_t es = SafeRead<uintptr_t>(client + Offsets::dwEntityList);
            if (!ValidPtr(es)) return;
            uintptr_t wc = SafeRead<uintptr_t>(es + 0x10 + ((h_w & 0x7FFF) >> 9) * 8);
            if (!ValidPtr(wc)) return;
            uintptr_t weap = SafeRead<uintptr_t>(wc + ((h_w & 0x7FFF) & 0x1FF) * 0x70);
            if (!ValidPtr(weap)) return;

            char d[64]{}; DesignerName(weap, d, sizeof(d));
            bool is_nade = std::strstr(d, "grenade") || std::strstr(d, "flashbang") || std::strstr(d, "molotov")
                || std::strstr(d, "incgrenade") || std::strstr(d, "decoy") || std::strstr(d, "smoke");
            if (!is_nade) return;

            Vec3 origin = SafeRead<Vec3>(local_pawn + Schema::m_vOldOrigin);
            Vec3 eye = origin; eye.z += 64.f;

            float pitch = SafeRead<float>(client + Offsets::dwViewAngles);
            float yaw = SafeRead<float>(client + Offsets::dwViewAngles + 4);
            constexpr float D2R = 0.0174532925f;
            float cp = std::cos(pitch * D2R), sp = std::sin(pitch * D2R);
            float cy = std::cos(yaw * D2R), sy = std::sin(yaw * D2R);

            float str = SafeRead<float>(weap + Schema::m_flThrowStrength);
            if (str < 0.f || str > 1.f) str = 1.f;

            // CS2 throw ~750 * strength, small upward bias when not full charge
            float spd = 750.f * str;
            Vec3 vel = { spd * cp * cy, spd * cp * sy, spd * -sp + (1.f - str) * 140.f };
            Vec3 pv = SafeRead<Vec3>(local_pawn + Schema::m_vecAbsVelocity);
            vel.x += pv.x * 0.5f;
            vel.y += pv.y * 0.5f;

            constexpr float G = 800.f, DT = 0.015f;
            constexpr int STEPS = 180;
            Vec3 pos = eye;
            Vec2 prev{};
            bool have = ToScreen(pos, prev, vm, W, H);
            float dist = 0.f;
            int bounces = 0;

            for (int i = 0; i < STEPS; ++i) {
                vel.z -= G * DT;
                Vec3 next = { pos.x + vel.x * DT, pos.y + vel.y * DT, pos.z + vel.z * DT };

                // ground
                if (next.z < origin.z) {
                    next.z = origin.z;
                    vel.z *= -0.38f;
                    vel.x *= 0.58f;
                    vel.y *= 0.58f;
                    if (std::fabs(vel.z) < 35.f) vel.z = 0.f;
                    if (++bounces > 6) break;
                }

                dist += Dist(pos, next);

                Vec2 sp;
                if (ToScreen(next, sp, vm, W, H)) {
                    if (have) {
                        // fade near the end
                        float a = 200.f * (1.f - (float)i / STEPS);
                        dl->AddLine(ImVec2(prev.x, prev.y), ImVec2(sp.x, sp.y), IM_COL32(255, 200, 40, (int)a), 2.f);
                    }
                    prev = sp; have = true;
                }
                else have = false;

                pos = next;
                if (vel.x == 0.f && vel.y == 0.f && vel.z == 0.f) break;
            }

            Vec2 land;
            if (ToScreen(pos, land, vm, W, H)) {
                dl->AddCircle(ImVec2(land.x, land.y), 7.f, IM_COL32(255, 200, 40, 240), 16, 2.f);
                dl->AddCircleFilled(ImVec2(land.x, land.y), 3.f, IM_COL32(255, 220, 80, 200));
                char lb[24];
                std::snprintf(lb, sizeof(lb), "%.0fm", dist * 0.0254f);
                ImVec2 ts = ImGui::CalcTextSize(lb);
                dl->AddText(ImVec2(land.x - ts.x * 0.5f, land.y + 10.f), IM_COL32(255, 230, 160, 255), lb);
            }
        }
    } // anonymous

    void PushDamage(const Vec3& pos, int dmg) {
        g_dmg.push_back({ pos, (float)ImGui::GetTime(), dmg });
        if (g_dmg.size() > 64) g_dmg.erase(g_dmg.begin());
    }

    void Render() {
        auto& v = g_cfg.visuals.player;
        auto& wld = g_cfg.visuals.world;
        bool any_player = v.esp_box || v.esp_name || v.esp_health || v.esp_armor || v.esp_weapon || v.esp_distance || v.esp_skeleton || v.glow || v.chams;
        bool any_world = wld.radar || wld.bomb_timer || wld.bomb_esp || wld.dropped_weapons || wld.grenade_pred || wld.damage_markers || wld.no_scope || v.glow_weapons || v.glow_bomb;
        if (!any_player && !any_world) return;

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
                Debug::g_esp.target_x = p.origin.x; Debug::g_esp.target_y = p.origin.y; Debug::g_esp.target_z = p.origin.z;
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
                if (p.health < 100) { char hbuf[8]; std::snprintf(hbuf, sizeof(hbuf), "%d", p.health); dl->AddText(ImVec2(l - 14.f, bot - h * f - 9.f), HpGradient(p.health), hbuf); }
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
                ImU32 sk = IM_COL32((int)(v.skel_color[0] * 255), (int)(v.skel_color[1] * 255), (int)(v.skel_color[2] * 255), 235);
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
    }
}