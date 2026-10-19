// --- esp.cpp ---
#include "esp.h"
#include "config.h"
#include "sdk.h"
#include "memory.h"
#include "entity.h"
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

        struct Vec2 { float x = 0.f, y = 0.f; };
        struct Mat4 { float m[4][4]{}; };

        // ── memory helpers ──────────────────────────────────────────────
        static bool ValidPtr(uintptr_t p) {
            return p > 0x10000 && p < 0x7FFFFFFFFFFF;
        }
        static bool IsFinite(const Vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
                && !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
        }
        static float Dist(const Vec3& a, const Vec3& b) {
            const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        static bool ToScreen(const Vec3& w, Vec2& out, const Mat4& vm, int W, int H) {
            const float cw = vm.m[3][0] * w.x + vm.m[3][1] * w.y
                + vm.m[3][2] * w.z + vm.m[3][3];
            if (!std::isfinite(cw) || cw < 0.001f) return false;
            const float cx = vm.m[0][0] * w.x + vm.m[0][1] * w.y
                + vm.m[0][2] * w.z + vm.m[0][3];
            const float cy = vm.m[1][0] * w.x + vm.m[1][1] * w.y
                + vm.m[1][2] * w.z + vm.m[1][3];
            const float inv = 1.f / cw;
            out.x = (W * 0.5f) * (cx * inv + 1.f);
            out.y = (H * 0.5f) * (1.f - cy * inv);
            return std::isfinite(out.x) && std::isfinite(out.y);
        }

        // ── bone layout ─────────────────────────────────────────────────
        namespace Bones {
            enum : int {
                PELVIS = 1, SPINE_0 = 2, SPINE_1 = 3, SPINE_2 = 4,
                NECK = 6, HEAD = 7,
                CLAV_L = 8, ARM_UP_L = 9, ARM_LO_L = 10, HAND_L = 11,
                CLAV_R = 12, ARM_UP_R = 13, ARM_LO_R = 14, HAND_R = 15,
                HIP_L = 17, KNEE_L = 18, ANKLE_L = 19,
                HIP_R = 20, KNEE_R = 21, ANKLE_R = 22,
                CHEST = 23
            };
            constexpr int MAX = 24;
            // CS2 bone stride = 0x20 (position in first 12 bytes, then pad)
            struct Joint { Vec3 pos; uint8_t pad[0x14]; };
            static_assert(sizeof(Joint) == 0x20, "CS2 bone stride mismatch");

            static const int links[][2] = {
                {HEAD, NECK}, {NECK, CHEST},
                {CHEST, SPINE_2}, {SPINE_2, SPINE_1}, {SPINE_1, PELVIS},
                {CHEST, CLAV_L}, {CLAV_L, ARM_UP_L}, {ARM_UP_L, ARM_LO_L}, {ARM_LO_L, HAND_L},
                {CHEST, CLAV_R}, {CLAV_R, ARM_UP_R}, {ARM_UP_R, ARM_LO_R}, {ARM_LO_R, HAND_R},
                {PELVIS, HIP_L}, {HIP_L, KNEE_L}, {KNEE_L, ANKLE_L},
                {PELVIS, HIP_R}, {HIP_R, KNEE_R}, {KNEE_R, ANKLE_R}
            };
            constexpr int LINK_COUNT = (int)(sizeof(links) / sizeof(links[0]));
        }

        // ── player data ─────────────────────────────────────────────────
        struct Player {
            uintptr_t pawn = 0;
            uintptr_t controller = 0;
            int index = 0;
            int team = 0;
            int health = 0;
            int armor = 0;
            float distance = 0.f;
            Vec3  origin{};
            Vec3  head{};
            Vec3  feet{};
            Vec3  bones[Bones::MAX]{};
            char  name[64]{};
            char  weapon[32]{};
            bool  scoped = false;
        };

        // ── utility ─────────────────────────────────────────────────────
        static void MemReadBuf(uintptr_t addr, void* out, size_t n) {
            if (!addr) { std::memset(out, 0, n); return; }
            auto* p = static_cast<uint8_t*>(out);
            size_t off = 0;
            while (n - off >= 8) { *reinterpret_cast<uint64_t*>(p + off) = Memory::Read<uint64_t>(addr + off); off += 8; }
            while (n - off >= 4) { *reinterpret_cast<uint32_t*>(p + off) = Memory::Read<uint32_t>(addr + off); off += 4; }
            while (n - off >= 2) { *reinterpret_cast<uint16_t*>(p + off) = Memory::Read<uint16_t>(addr + off); off += 2; }
            while (n - off >= 1) { *reinterpret_cast<uint8_t*>(p + off) = Memory::Read<uint8_t>(addr + off); off += 1; }
        }
        static void Sanitize(char* s, size_t n) {
            if (!s || !n) return;
            s[n - 1] = 0;
            for (size_t i = 0; s[i]; ++i)
                if ((unsigned char)s[i] < 0x20) s[i] = ' ';
        }
        static void DesignerName(uintptr_t entity, char* out, size_t n) {
            if (!out || !n) return;
            out[0] = 0;
            if (!ValidPtr(entity)) return;
            const uintptr_t identity = Memory::Read<uintptr_t>(entity + Schema::m_Identity);
            if (!ValidPtr(identity)) return;
            const uintptr_t str = Memory::Read<uintptr_t>(identity + Schema::m_designerName);
            if (!ValidPtr(str)) return;
            MemReadBuf(str, out, n - 1);
            out[n - 1] = 0;
        }
        static ImU32 HpGradient(int hp) {
            const float t = std::clamp(hp / 100.f, 0.f, 1.f);
            const float r = t < 0.5f ? 1.f : (1.f - t) * 2.f;
            const float g = t < 0.5f ? t * 2.f : 1.f;
            return IM_COL32((int)(r * 255), (int)(g * 255), 50, 255);
        }

        // ── entity list ─────────────────────────────────────────────────
        struct EntityList {
            std::vector<Player> players;
            int local_team = 0;
            int dbg_update_count = 0;

            void Update(uintptr_t local_pawn, uintptr_t entity_system) {
                players.clear();
                players.reserve(32);
                local_team = 0;
                dbg_update_count++;
                if (!ValidPtr(entity_system)) return;

                int max_index = Memory::Read<int>(entity_system + 0x2120);
                if (max_index <= 0 || max_index > 8192) max_index = 4096;

                Vec3 local_origin{};
                if (ValidPtr(local_pawn)) {
                    local_origin = Memory::Read<Vec3>(local_pawn + Schema::m_vOldOrigin);
                    local_team = Memory::Read<uint8_t>(local_pawn + Schema::m_iTeamNum);
                }

                for (int i = 1; i <= max_index; ++i) {
                    const uintptr_t chunk = Memory::Read<uintptr_t>(
                        entity_system + 0x10 + (i >> 9) * 8);
                    if (!ValidPtr(chunk)) continue;

                    const uintptr_t ent = Memory::Read<uintptr_t>(
                        chunk + (i & 0x1FF) * 0x78);
                    if (!ValidPtr(ent) || ent == local_pawn) continue;

                    const int hp = Memory::Read<int>(ent + Schema::m_iHealth);
                    if (hp <= 0 || hp > 100) continue;

                    const int team = Memory::Read<uint8_t>(ent + Schema::m_iTeamNum);
                    if (team != 2 && team != 3) continue;

                    if (Memory::Read<uint8_t>(ent + Schema::m_lifeState) != 0) continue;

                    const uintptr_t node = Memory::Read<uintptr_t>(ent + Schema::m_pGameSceneNode);
                    if (!ValidPtr(node)) continue;
                    if (Memory::Read<bool>(node + Schema::m_bDormant)) continue;

                    // bone array: node + m_modelState + 0x80
                    const uintptr_t bone_arr = Memory::Read<uintptr_t>(
                        node + Schema::m_modelState + Schema::m_pBoneArray);
                    if (!ValidPtr(bone_arr)) continue;

                    Bones::Joint joints[Bones::MAX]{};
                    MemReadBuf(bone_arr, joints, sizeof(joints));
                    if (!IsFinite(joints[Bones::HEAD].pos)) continue;

                    Player p{};
                    p.pawn = ent;
                    p.index = i;
                    p.team = team;
                    p.health = hp;
                    p.armor = std::clamp(Memory::Read<int>(ent + Schema::m_ArmorValue), 0, 100);
                    p.origin = Memory::Read<Vec3>(ent + Schema::m_vOldOrigin);
                    p.scoped = Memory::Read<bool>(ent + Schema::m_bIsScoped);
                    if (!IsFinite(p.origin)) continue;

                    for (int b = 0; b < Bones::MAX; ++b) p.bones[b] = joints[b].pos;

                    p.head = joints[Bones::HEAD].pos;
                    p.head.z += 6.f;

                    const Vec3 al = joints[Bones::ANKLE_L].pos;
                    const Vec3 ar = joints[Bones::ANKLE_R].pos;
                    const bool vl = IsFinite(al), vr = IsFinite(ar);
                    if (vl && vr)  p.feet = (al.z < ar.z) ? al : ar;
                    else if (vl)   p.feet = al;
                    else if (vr)   p.feet = ar;
                    else { p.feet = joints[Bones::PELVIS].pos; p.feet.z -= 34.f; }
                    p.feet.z -= 2.f;

                    // controller → name
                    const uint32_t h_ctrl = Memory::Read<uint32_t>(ent + Schema::m_hController);
                    if (h_ctrl && h_ctrl != 0xFFFFFFFF) {
                        const uintptr_t cc = Memory::Read<uintptr_t>(
                            entity_system + 0x10 + ((h_ctrl & 0x7FFF) >> 9) * 8);
                        if (ValidPtr(cc)) {
                            const uintptr_t ctrl = Memory::Read<uintptr_t>(
                                cc + ((h_ctrl & 0x7FFF) & 0x1FF) * 0x78);
                            if (ValidPtr(ctrl)) {
                                p.controller = ctrl;
                                MemReadBuf(ctrl + Schema::m_sSanitizedPlayerName,
                                    p.name, sizeof(p.name) - 1);
                                Sanitize(p.name, sizeof(p.name));
                            }
                        }
                    }

                    // weapon services → active weapon → designer name
                    const uintptr_t ws = Memory::Read<uintptr_t>(ent + Schema::m_pWeaponServices);
                    if (ValidPtr(ws)) {
                        const uint32_t h_w = Memory::Read<uint32_t>(ws + Schema::m_hActiveWeapon);
                        if (h_w && h_w != 0xFFFFFFFF) {
                            const uintptr_t wc = Memory::Read<uintptr_t>(
                                entity_system + 0x10 + ((h_w & 0x7FFF) >> 9) * 8);
                            if (ValidPtr(wc)) {
                                const uintptr_t weap = Memory::Read<uintptr_t>(
                                    wc + ((h_w & 0x7FFF) & 0x1FF) * 0x78);
                                if (ValidPtr(weap)) {
                                    char d[64]{};
                                    DesignerName(weap, d, sizeof(d));
                                    if (std::strncmp(d, "weapon_", 7) == 0)
                                        std::snprintf(p.weapon, sizeof(p.weapon), "%s", d + 7);
                                    else if (std::strncmp(d, "knife", 5) == 0)
                                        std::snprintf(p.weapon, sizeof(p.weapon), "Knife");
                                    else if (std::strncmp(d, "c4", 2) == 0)
                                        std::snprintf(p.weapon, sizeof(p.weapon), "C4");
                                }
                            }
                        }
                    }

                    p.distance = Dist(local_origin, p.origin);
                    players.push_back(p);
                }
            }
        };

        // ── render helpers ──────────────────────────────────────────────
        static float ReadCurtime(uintptr_t client) {
            const uintptr_t gv = Memory::Read<uintptr_t>(client + Offsets::dwGlobalVars);
            if (!ValidPtr(gv)) return 0.f;
            return Memory::Read<float>(gv + 0x2C); // curtime inside CGlobalVarsBase
        }

        // ── bomb timer ──────────────────────────────────────────────────
        static void DrawBombTimer(ImDrawList* dl, int W, uintptr_t client) {
            const uintptr_t c4 = Memory::Read<uintptr_t>(client + Offsets::dwPlantedC4);
            if (!ValidPtr(c4)) return;
            if (!Memory::Read<bool>(c4 + Schema::m_bBombTicking)) return;

            const float blow = Memory::Read<float>(c4 + Schema::m_flC4Blow);
            const float curtime = ReadCurtime(client);
            const float remaining = blow - curtime;
            if (remaining <= 0.f || remaining > 60.f) return;

            const bool defusing = Memory::Read<bool>(c4 + Schema::m_bBeingDefused);
            const float def_end = Memory::Read<float>(c4 + Schema::m_flDefuseCountDown);
            const float def_rem = defusing ? (def_end - curtime) : -1.f;
            const int   site = Memory::Read<int>(c4 + Schema::m_nBombSite);
            const bool  critical = remaining < 5.f;
            const float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime()
                * (critical ? 10.f : 2.8f));

            constexpr float CW = 230.f, CH = defusing ? 68.f : 52.f;
            const float cx = W * 0.5f, cy = 62.f;
            const ImVec2 p0(cx - CW * 0.5f, cy), p1(cx + CW * 0.5f, cy + CH);

            dl->AddRectFilled(p0, p1, IM_COL32(10, 12, 18, 245), 5.f);
            dl->AddRect(p0, p1,
                critical ? IM_COL32(210, 65, 55, 255) : IM_COL32(38, 140, 242, 255),
                5.f, 0, 1.2f + pulse * (critical ? 1.2f : 0.f));

            // progress bar
            const float bar_w = CW - 12.f;
            const float bar_x = cx - bar_w * 0.5f;
            const float bar_y = cy + 6.f;
            const float fuse = 40.f; // CS2 default fuse time
            const float prog = std::clamp(remaining / fuse, 0.f, 1.f);
            dl->AddRectFilled(ImVec2(bar_x, bar_y),
                ImVec2(bar_x + bar_w, bar_y + 6.f), IM_COL32(25, 28, 35, 255), 3.f);
            dl->AddRectFilled(ImVec2(bar_x, bar_y),
                ImVec2(bar_x + bar_w * prog, bar_y + 6.f),
                critical ? IM_COL32(220, 60, 50, 255) : IM_COL32(38, 140, 242, 255), 3.f);

            char buf[48];
            std::snprintf(buf, sizeof(buf), "C4 [%s]  %.1fs",
                site == 1 ? "B" : "A", remaining);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(cx - ts.x * 0.5f, bar_y + 10.f),
                critical ? IM_COL32(255, 90, 80, 255) : IM_COL32(230, 235, 245, 255), buf);

            if (defusing && def_rem > 0.f) {
                char dbuf[48];
                std::snprintf(dbuf, sizeof(dbuf), "defusing  %.1fs", def_rem);
                const ImVec2 ds = ImGui::CalcTextSize(dbuf);
                dl->AddText(ImVec2(cx - ds.x * 0.5f, bar_y + 26.f),
                    IM_COL32(80, 220, 150, 255), dbuf);
            }
        }

        // ── radar ───────────────────────────────────────────────────────
        static void DrawRadar(ImDrawList* dl, int W,
            const std::vector<Player>& players,
            const Vec3& local_origin, int local_team,
            const Mat4& vm, int cfg_pos, int cfg_zoom) {
            if (!IsFinite(local_origin)) return;
            constexpr float BASE_SIZE = 260.f;
            const float zoom_scale = 1.f + cfg_zoom * 0.01f;   // 0–100 → 1.0–2.0x
            const float RANGE = 6500.f / zoom_scale;
            const float PADDING = 16.f;
            constexpr float RS = BASE_SIZE;

            float rx;
            switch (cfg_pos) {
            case 0:  rx = PADDING;                  break; // left
            case 2:  rx = (W - RS) * 0.5f;         break; // center
            default: rx = W - RS - PADDING;         break; // right
            }
            const float ry = PADDING;
            const float cx = rx + RS * 0.5f, cy = ry + RS * 0.5f;
            const float scale = (RS * 0.46f) / RANGE;

            const float fx = -vm.m[0][2], fy = -vm.m[1][2];
            const float yaw = std::atan2(fy, fx);

            dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + RS, ry + RS),
                IM_COL32(12, 14, 18, 240), 8.f);
            dl->AddRect(ImVec2(rx, ry), ImVec2(rx + RS, ry + RS),
                IM_COL32(50, 160, 230, 200), 8.f, 0, 1.8f);

            // local player arrow
            const float dx = std::cos(yaw), dy = -std::sin(yaw);
            dl->AddTriangleFilled(
                ImVec2(cx + dx * 10.f, cy + dy * 10.f),
                ImVec2(cx - dy * 6.5f - dx * 5.5f, cy + dx * 6.5f - dy * 5.5f),
                ImVec2(cx + dy * 6.5f - dx * 5.5f, cy - dx * 6.5f - dy * 5.5f),
                IM_COL32(100, 235, 255, 255));

            for (const auto& p : players) {
                const float ddx = p.origin.x - local_origin.x;
                const float ddy = p.origin.y - local_origin.y;
                if (std::sqrt(ddx * ddx + ddy * ddy) > RANGE) continue;
                const float px = std::clamp(cx + ddx * scale, rx + 8.f, rx + RS - 8.f);
                const float py = std::clamp(cy - ddy * scale, ry + 8.f, ry + RS - 8.f);
                const ImU32 col = (p.team == local_team)
                    ? IM_COL32(70, 220, 140, 255)
                    : IM_COL32(245, 75, 75, 255);
                dl->AddCircleFilled(ImVec2(px, py), 4.5f, col, 16);
            }
            dl->AddText(ImVec2(rx + 10.f, ry + 7.f), IM_COL32(170, 215, 255, 230), "RADAR");
        }

        // ── dropped weapons ─────────────────────────────────────────────
        // iterate entity list looking for weapon_ entities without owner
        static void DrawDroppedWeapons(ImDrawList* dl, int W, int H,
            const Mat4& vm, uintptr_t entity_system,
            uintptr_t local_pawn) {
            if (!ValidPtr(entity_system)) return;
            const int max_index = []() -> int {
                const uintptr_t client = Memory::GetModuleBase("client.dll");
                if (!client) return 512;
                const uintptr_t es = Memory::Read<uintptr_t>(client + Offsets::dwEntityList);
                if (!ValidPtr(es)) return 512;
                const int m = Memory::Read<int>(es + 0x2120);
                return (m > 0 && m <= 8192) ? m : 512;
                }();

            Vec3 local_origin{};
            if (ValidPtr(local_pawn))
                local_origin = Memory::Read<Vec3>(local_pawn + Schema::m_vOldOrigin);

            for (int i = 1; i <= max_index; ++i) {
                const uintptr_t chunk = Memory::Read<uintptr_t>(
                    entity_system + 0x10 + (i >> 9) * 8);
                if (!ValidPtr(chunk)) continue;
                const uintptr_t ent = Memory::Read<uintptr_t>(
                    chunk + (i & 0x1FF) * 0x78);
                if (!ValidPtr(ent) || ent == local_pawn) continue;

                // must have no owner (not held by anyone)
                const uint32_t owner = Memory::Read<uint32_t>(ent + 0x520); // m_hOwnerEntity
                if (owner != 0xFFFFFFFF && owner != 0) continue;

                char d[64]{};
                DesignerName(ent, d, sizeof(d));
                if (std::strncmp(d, "weapon_", 7) != 0) continue;

                const uintptr_t node = Memory::Read<uintptr_t>(ent + Schema::m_pGameSceneNode);
                if (!ValidPtr(node)) continue;
                if (Memory::Read<bool>(node + Schema::m_bDormant)) continue;

                const Vec3 origin = Memory::Read<Vec3>(node + Schema::m_vecAbsOrigin);
                if (!IsFinite(origin)) continue;

                Vec3 above = origin; above.z += 12.f;
                Vec2 sp;
                if (!ToScreen(above, sp, vm, W, H)) continue;
                if (sp.x < 0.f || sp.x > W || sp.y < 0.f || sp.y > H) continue;

                const float dist = Dist(local_origin, origin) * 0.0254f; // units → meters
                char label[80];
                std::snprintf(label, sizeof(label), "%s  %.0fm", d + 7, dist);
                const ImVec2 ts = ImGui::CalcTextSize(label);
                const float tx = std::clamp(sp.x - ts.x * 0.5f, 2.f, (float)W - ts.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, sp.y + 1.f), IM_COL32(0, 0, 0, 160), label);
                dl->AddText(ImVec2(tx, sp.y), IM_COL32(240, 210, 100, 255), label);
            }
        }

        // ── damage markers ──────────────────────────────────────────────
        struct DmgMarker {
            Vec3  world_pos{};
            float spawn_time = 0.f;
            int   damage = 0;
        };
        static std::vector<DmgMarker> g_dmg_markers;

        void PushDamageMarker(const Vec3& pos, int damage) {
            g_dmg_markers.push_back({ pos, (float)ImGui::GetTime(), damage });
            if (g_dmg_markers.size() > 64) g_dmg_markers.erase(g_dmg_markers.begin());
        }

        static void DrawDamageMarkers(ImDrawList* dl, int W, int H, const Mat4& vm) {
            const float now = (float)ImGui::GetTime();
            constexpr float LIFETIME = 1.4f;
            for (auto it = g_dmg_markers.begin(); it != g_dmg_markers.end(); ) {
                const float age = now - it->spawn_time;
                if (age > LIFETIME) { it = g_dmg_markers.erase(it); continue; }
                const float t = age / LIFETIME;        // 0 → 1
                const float alpha = 1.f - t;
                Vec3 up = it->world_pos; up.z += 18.f + t * 28.f; // float upward
                Vec2 sp;
                if (ToScreen(up, sp, vm, W, H)) {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "-%d", it->damage);
                    const ImVec2 ts = ImGui::CalcTextSize(buf);
                    const float tx = sp.x - ts.x * 0.5f;
                    const ImU32 col = IM_COL32(255, 80, 60, (int)(alpha * 255));
                    const ImU32 shadow = IM_COL32(0, 0, 0, (int)(alpha * 160));
                    dl->AddText(ImVec2(tx + 1.f, sp.y + 1.f), shadow, buf);
                    dl->AddText(ImVec2(tx, sp.y), col, buf);
                }
                ++it;
            }
        }

        // ── no scope overlay (cross-overlay removed) ─────────────────────
        static void DrawNoScopeOverlay(ImDrawList* dl, int W, int H, bool is_scoped) {
            if (!is_scoped) return;
            // draw black bars to remove scope border — leave crosshair area clear
            constexpr float RING = 0.22f;  // fraction of screen to keep clear
            const float ry = H * RING, rx = W * RING;
            // top / bottom bars
            dl->AddRectFilled(ImVec2(0.f, 0.f), ImVec2((float)W, ry), IM_COL32(0, 0, 0, 255));
            dl->AddRectFilled(ImVec2(0.f, H - ry), ImVec2((float)W, (float)H), IM_COL32(0, 0, 0, 255));
            // left / right bars
            dl->AddRectFilled(ImVec2(0.f, ry), ImVec2(rx, H - ry), IM_COL32(0, 0, 0, 255));
            dl->AddRectFilled(ImVec2(W - rx, ry), ImVec2((float)W, H - ry), IM_COL32(0, 0, 0, 255));
            // thin crosshair lines
            const float cx = W * 0.5f, cy = H * 0.5f;
            dl->AddLine(ImVec2(rx, cy), ImVec2(W - rx, cy), IM_COL32(255, 255, 255, 200), 1.2f);
            dl->AddLine(ImVec2(cx, ry), ImVec2(cx, H - ry), IM_COL32(255, 255, 255, 200), 1.2f);
        }

        // ── grenade prediction ──────────────────────────────────────────
        // Reads localplayer's held grenade throw params and simulates arc.
        // Uses dwCSGOInput → view angles + velocity for trajectory calc.
        static void DrawGrenadePrediction(ImDrawList* dl, int W, int H,
            const Mat4& vm, uintptr_t local_pawn,
            uintptr_t client) {
            if (!ValidPtr(local_pawn)) return;
            // is local holding a grenade?
            const uintptr_t ws = Memory::Read<uintptr_t>(local_pawn + Schema::m_pWeaponServices);
            if (!ValidPtr(ws)) return;
            const uint32_t h_w = Memory::Read<uint32_t>(ws + Schema::m_hActiveWeapon);
            if (!h_w || h_w == 0xFFFFFFFF) return;

            const uintptr_t entity_system = Memory::Read<uintptr_t>(client + Offsets::dwEntityList);
            if (!ValidPtr(entity_system)) return;
            const uintptr_t wc = Memory::Read<uintptr_t>(
                entity_system + 0x10 + ((h_w & 0x7FFF) >> 9) * 8);
            if (!ValidPtr(wc)) return;
            const uintptr_t weap = Memory::Read<uintptr_t>(
                wc + ((h_w & 0x7FFF) & 0x1FF) * 0x78);
            if (!ValidPtr(weap)) return;

            char d[64]{};
            DesignerName(weap, d, sizeof(d));
            // only predict actual grenade types
            const bool is_nade = (std::strstr(d, "grenade") || std::strstr(d, "flashbang")
                || std::strstr(d, "smokegrenade") || std::strstr(d, "molotov")
                || std::strstr(d, "incgrenade") || std::strstr(d, "decoy"));
            if (!is_nade) return;

            // eye position
            const Vec3 origin = Memory::Read<Vec3>(local_pawn + Schema::m_vOldOrigin);
            Vec3 eye = origin; eye.z += 64.f;

            // view angles from dwViewAngles (QAngle pitch/yaw in degrees)
            const uintptr_t input_ptr = Memory::Read<uintptr_t>(client + Offsets::dwCSGOInput);
            if (!ValidPtr(input_ptr)) return;
            // QAngle is at dwViewAngles offset from client base, not from input ptr
            const uintptr_t va_ptr = client + Offsets::dwViewAngles;
            const float pitch = Memory::Read<float>(va_ptr);      // degrees
            const float yaw = Memory::Read<float>(va_ptr + 4);

            constexpr float DEG2RAD = 3.14159265f / 180.f;
            const float cp = std::cos(pitch * DEG2RAD), sp2 = std::sin(pitch * DEG2RAD);
            const float cy2 = std::cos(yaw * DEG2RAD), sy = std::sin(yaw * DEG2RAD);

            // CS2 throw speed ≈ 750 u/s (full pull)
            constexpr float SPEED = 750.f;
            constexpr float GRAVITY = 800.f;   // sv_gravity default
            constexpr float DT = 0.016f;  // simulation step
            constexpr int   STEPS = 200;

            Vec3 pos = eye;
            Vec3 vel = { SPEED * cp * cy2, SPEED * cp * sy, SPEED * -sp2 };

            // add local player velocity (first 2 axes)
            const Vec3 pvel = Memory::Read<Vec3>(local_pawn + Schema::m_vecAbsVelocity);
            vel.x += pvel.x * 0.5f;
            vel.y += pvel.y * 0.5f;

            Vec2 prev_sp;
            bool prev_valid = ToScreen(pos, prev_sp, vm, W, H);

            for (int i = 0; i < STEPS; ++i) {
                vel.z -= GRAVITY * DT;
                pos.x += vel.x * DT;
                pos.y += vel.y * DT;
                pos.z += vel.z * DT;

                // rough bounce / stop on z floor (no BSP, just approximate)
                if (pos.z < origin.z - 1.f) {
                    pos.z = origin.z;
                    vel.z *= -0.45f;
                    vel.x *= 0.65f;
                    vel.y *= 0.65f;
                    if (std::fabs(vel.z) < 32.f) { vel.z = 0.f; }
                }

                Vec2 sp;
                if (!ToScreen(pos, sp, vm, W, H)) { prev_valid = false; continue; }
                if (sp.x < 0.f || sp.x > W || sp.y < 0.f || sp.y > H) { prev_valid = false; continue; }
                if (prev_valid)
                    dl->AddLine(ImVec2(prev_sp.x, prev_sp.y), ImVec2(sp.x, sp.y),
                        IM_COL32(255, 210, 40, 180), 1.6f);
                prev_sp = sp;
                prev_valid = true;

                if (vel.x == 0.f && vel.y == 0.f && vel.z == 0.f) break;
            }
            // landing dot
            Vec2 land;
            if (ToScreen(pos, land, vm, W, H))
                dl->AddCircle(ImVec2(land.x, land.y), 5.f, IM_COL32(255, 210, 40, 220), 16, 1.6f);
        }

    } // anonymous namespace

    // ── Render ──────────────────────────────────────────────────────────────────
    void Render() {
        auto& v = g_cfg.visuals.player;
        auto& wld = g_cfg.visuals.world;

        const bool any_player = v.esp_box || v.esp_name || v.esp_health ||
            v.esp_armor || v.esp_weapon || v.esp_distance ||
            v.esp_skeleton;
        const bool any_world = wld.radar || wld.bomb_timer ||
            wld.dropped_weapons || wld.grenade_pred ||
            wld.damage_markers || wld.no_scope;
        if (!any_player && !any_world) return;

        const uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;
        ImGuiIO& io = ImGui::GetIO();
        const int W = (int)io.DisplaySize.x, H = (int)io.DisplaySize.y;

        const Mat4      vm = Memory::Read<Mat4>(client + Offsets::dwViewMatrix);
        const uintptr_t local = Memory::Read<uintptr_t>(client + Offsets::dwLocalPlayerPawn);
        const uintptr_t es = Memory::Read<uintptr_t>(client + Offsets::dwEntityList);

        // ── player ESP ──────────────────────────────────────────────────
        static EntityList entities;
        entities.Update(local, es);

        Vec3 local_origin{};
        if (ValidPtr(local)) local_origin = Memory::Read<Vec3>(local + Schema::m_vOldOrigin);

        const int local_team = entities.local_team;
        bool local_scoped = false;
        if (ValidPtr(local))
            local_scoped = Memory::Read<bool>(local + Schema::m_bIsScoped);

        Debug::g_esp.entities_found = (int)entities.players.size();
        Debug::g_esp.entities_on_screen = 0;
        Debug::g_esp.local_x = local_origin.x;
        Debug::g_esp.local_y = local_origin.y;
        Debug::g_esp.local_z = local_origin.z;

        for (const auto& p : entities.players) {
            Vec2 hs, fs;
            if (!ToScreen(p.head, hs, vm, W, H)) continue;
            if (!ToScreen(p.feet, fs, vm, W, H)) continue;
            const float top = std::min(hs.y, fs.y);
            const float bot = std::max(hs.y, fs.y);
            const float h = bot - top;
            if (h < 4.f || h > 2000.f) continue;

            // box width from clavicles
            float l, r;
            Vec2 cl, cr;
            if (ToScreen(p.bones[Bones::CLAV_L], cl, vm, W, H) &&
                ToScreen(p.bones[Bones::CLAV_R], cr, vm, W, H)) {
                const float mx = (cl.x + cr.x) * 0.5f;
                float bw = std::fabs(cr.x - cl.x) * 1.35f;
                bw = std::clamp(bw, h * 0.40f, h * 0.70f);
                l = mx - bw * 0.5f; r = mx + bw * 0.5f;
            }
            else {
                const float bw = h * 0.45f;
                l = hs.x - bw * 0.5f; r = hs.x + bw * 0.5f;
            }
            if (r < 0.f || l > W || bot < 0.f || top > H) continue;

            Debug::g_esp.entities_on_screen++;
            if (Debug::g_esp.entities_on_screen == 1) {
                Debug::g_esp.target_x = p.origin.x;
                Debug::g_esp.target_y = p.origin.y;
                Debug::g_esp.target_z = p.origin.z;
            }

            const bool  enemy = (p.team != local_team);
            const ImU32 col = enemy
                ? IM_COL32(235, 72, 72, 255)
                : IM_COL32(86, 214, 140, 255);

            if (v.esp_box) {
                dl->AddRect(ImVec2(l - 1.f, top - 1.f), ImVec2(r + 1.f, bot + 1.f),
                    IM_COL32(0, 0, 0, 90), 0.f, 0, 1.f);
                dl->AddRect(ImVec2(l, top), ImVec2(r, bot), col, 0.f, 0, 1.25f);
            }

            // health bar (left side)
            if (v.esp_health) {
                const float f = std::clamp(p.health / 100.f, 0.f, 1.f);
                dl->AddRectFilled(ImVec2(l - 6.f, top), ImVec2(l - 3.f, bot),
                    IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled(ImVec2(l - 6.f, bot - h * f), ImVec2(l - 3.f, bot),
                    HpGradient(p.health));
                // numeric label if health < 100
                if (p.health < 100) {
                    char hbuf[8]; std::snprintf(hbuf, sizeof(hbuf), "%d", p.health);
                    dl->AddText(ImVec2(l - 14.f, bot - h * f - 9.f),
                        HpGradient(p.health), hbuf);
                }
            }

            // armor bar (right side)
            if (v.esp_armor && p.armor > 0) {
                const float f = std::clamp(p.armor / 100.f, 0.f, 1.f);
                dl->AddRectFilled(ImVec2(r + 3.f, top), ImVec2(r + 6.f, bot),
                    IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled(ImVec2(r + 3.f, bot - h * f), ImVec2(r + 6.f, bot),
                    IM_COL32(80, 160, 255, 230));
            }

            // name
            if (v.esp_name && p.name[0]) {
                const ImVec2 sz = ImGui::CalcTextSize(p.name);
                const float  tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, top - sz.y - 3.f + 1.f),
                    IM_COL32(0, 0, 0, 180), p.name);
                dl->AddText(ImVec2(tx, top - sz.y - 3.f),
                    IM_COL32(245, 245, 245, 255), p.name);
            }

            // weapon + distance stacked below box
            float below = bot + 3.f;
            if (v.esp_weapon && p.weapon[0]) {
                const ImVec2 sz = ImGui::CalcTextSize(p.weapon);
                const float  tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, below + 1.f), IM_COL32(0, 0, 0, 180), p.weapon);
                dl->AddText(ImVec2(tx, below), IM_COL32(215, 215, 215, 255), p.weapon);
                below += sz.y + 1.f;
            }
            if (v.esp_distance) {
                char dbuf[24];
                std::snprintf(dbuf, sizeof(dbuf), "%.1fm", p.distance * 0.0254f);
                const ImVec2 sz = ImGui::CalcTextSize(dbuf);
                const float  tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, below + 1.f), IM_COL32(0, 0, 0, 180), dbuf);
                dl->AddText(ImVec2(tx, below), IM_COL32(190, 190, 190, 255), dbuf);
            }

            // skeleton
            if (v.esp_skeleton) {
                const ImU32 sk = IM_COL32(
                    (int)(v.skel_color[0] * 255), (int)(v.skel_color[1] * 255),
                    (int)(v.skel_color[2] * 255), 235);
                for (int li = 0; li < Bones::LINK_COUNT; ++li) {
                    const int b0 = Bones::links[li][0], b1 = Bones::links[li][1];
                    if (!IsFinite(p.bones[b0]) || !IsFinite(p.bones[b1])) continue;
                    Vec2 a, b;
                    if (!ToScreen(p.bones[b0], a, vm, W, H)) continue;
                    if (!ToScreen(p.bones[b1], b, vm, W, H)) continue;
                    dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), sk, 1.15f);
                }
            }
        }

        // ── world visuals ───────────────────────────────────────────────
        if (wld.radar)
            DrawRadar(dl, W, entities.players, local_origin, local_team, vm,
                wld.radar_pos, wld.radar_zoom);

        if (wld.bomb_timer)
            DrawBombTimer(dl, W, client);

        if (wld.dropped_weapons)
            DrawDroppedWeapons(dl, W, H, vm, es, local);

        if (wld.grenade_pred)
            DrawGrenadePrediction(dl, W, H, vm, local, client);

        if (wld.damage_markers)
            DrawDamageMarkers(dl, W, H, vm);

        if (wld.no_scope)
            DrawNoScopeOverlay(dl, W, H, local_scoped);
    }

} // namespace Features::Visuals::ESP