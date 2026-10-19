// language: C++, file: src/esp.cpp
// Escaneo robusto, sin depender de highestEntityIndex. Logs de diagnóstico.
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
#include <cctype>
#include <cstdint>

namespace Features::Visuals::ESP {
    namespace {

        struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };
        struct Vec2 { float x = 0.f, y = 0.f; };
        struct Mat4 { float m[4][4]{}; };

        static bool MemReadBuf(uintptr_t addr, void* out, size_t n) {
            if (!addr) { std::memset(out, 0, n); return false; }
            auto* p = static_cast<uint8_t*>(out);
            size_t off = 0;
            while (n - off >= 8) { *reinterpret_cast<uint64_t*>(p + off) = Memory::Read<uint64_t>(addr + off); off += 8; }
            while (n - off >= 4) { *reinterpret_cast<uint32_t*>(p + off) = Memory::Read<uint32_t>(addr + off); off += 4; }
            while (n - off >= 2) { *reinterpret_cast<uint16_t*>(p + off) = Memory::Read<uint16_t>(addr + off); off += 2; }
            while (n - off >= 1) { *reinterpret_cast<uint8_t*>(p + off) = Memory::Read<uint8_t>(addr + off);  off += 1; }
            return true;
        }

        namespace Offsets {
            constexpr uintptr_t dwEntityList = 0x271A908;
            constexpr uintptr_t dwLocalPlayerPawn = 0x2565718;
            constexpr uintptr_t dwViewMatrix = 0x256B020;
            constexpr uintptr_t dwPlantedC4 = 0x24CB3C0;
            constexpr uintptr_t dwGlobalVars = 0x2230090;
        }

        namespace Schema {
            static int m_iHealth = 0x34C;
            static int m_iTeamNum = 0x3E7;
            static int m_lifeState = 0x354;
            static int m_pGameSceneNode = 0x330;
            static int m_pBoneArray = 0x1C0;
            static int m_bDormant = 0x103;
            static int m_hController = 0x14BC;
            static int m_vOldOrigin = 0x14A4;
            static int m_sSanitizedPlayerName = 0x878;
            static int m_iPawnArmor = 0x1EC8;
            static int m_pWeaponServices = 0x12F0;
            static int m_hActiveWeapon = 0x60;
            static int m_Identity = 0x10;
            static int m_designerName = 0x20;
            static int m_bBombTicking = 0x1288;
            static int m_flC4Blow = 0x12B8;
            static int m_nBombSite = 0x128C;
        }

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
            struct Joint { Vec3 pos; char pad[0x14]; };
            static_assert(sizeof(Joint) == 0x20, "CS2 bone stride must be 0x20");
        }

        struct Player {
            uintptr_t controller = 0;
            uintptr_t pawn = 0;
            int index = 0;
            int team = 0;
            int health = 0;
            int armor = 0;
            Vec3 origin{};
            Vec3 head{};
            Vec3 feet{};
            Vec3 bones[Bones::MAX]{};
            float distance = 0.f;
            char name[64]{};
            char weapon[32]{};
        };

        static bool ValidPtr(uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFF; }
        static bool IsFinite(const Vec3& v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
                && !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
        }
        static float Dist(const Vec3& a, const Vec3& b) {
            const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        static bool ToScreen(const Vec3& w, Vec2& out, const Mat4& vm, int W, int H) {
            const float cw = vm.m[3][0] * w.x + vm.m[3][1] * w.y + vm.m[3][2] * w.z + vm.m[3][3];
            if (!std::isfinite(cw) || cw < 0.001f) return false;
            const float cx = vm.m[0][0] * w.x + vm.m[0][1] * w.y + vm.m[0][2] * w.z + vm.m[0][3];
            const float cy = vm.m[1][0] * w.x + vm.m[1][1] * w.y + vm.m[1][2] * w.z + vm.m[1][3];
            const float inv = 1.f / cw;
            out.x = (W * 0.5f) * (cx * inv + 1.f);
            out.y = (H * 0.5f) * (1.f - cy * inv);
            return std::isfinite(out.x) && std::isfinite(out.y);
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

        static void Sanitize(char* s, size_t n) {
            if (!s || !n) return;
            s[n - 1] = 0;
            for (size_t i = 0; s[i]; ++i)
                if ((unsigned char)s[i] < 0x20) s[i] = ' ';
        }

        struct EntityList {
            std::vector<Player> players;
            int local_team = 0;

            // diagnóstico
            uintptr_t dbg_es = 0;
            int dbg_max_index = 0;
            int dbg_chunks_ok = 0;
            int dbg_ent_found = 0;
            int dbg_hp_ok = 0;
            int dbg_team_ok = 0;
            int dbg_update_count = 0;

            void Update(uintptr_t local_pawn) {
                players.clear();
                players.reserve(32);
                local_team = 0;
                dbg_es = 0;
                dbg_max_index = 0;
                dbg_chunks_ok = 0;
                dbg_ent_found = 0;
                dbg_hp_ok = 0;
                dbg_team_ok = 0;
                dbg_update_count++;

                const uintptr_t client = Memory::GetModuleBase("client.dll");
                if (!client) return;

                const uintptr_t entity_system = Memory::Read<uintptr_t>(client + Offsets::dwEntityList);
                dbg_es = entity_system;
                if (!ValidPtr(entity_system)) return;

                // max_index: dentro del EntitySystem en +0x2120 (per dump)
                int max_index = Memory::Read<int>(entity_system + 0x2120);
                dbg_max_index = max_index;
                if (max_index <= 0 || max_index > 8192) max_index = 4096;

                Vec3 local_origin{};
                if (ValidPtr(local_pawn)) {
                    local_origin = Memory::Read<Vec3>(local_pawn + Schema::m_vOldOrigin);
                    local_team = Memory::Read<uint8_t>(local_pawn + Schema::m_iTeamNum);
                }

                for (int i = 1; i <= max_index; ++i) {
                    const uintptr_t chunk = Memory::Read<uintptr_t>(entity_system + 0x10 + (i >> 9) * 8);
                    if (!ValidPtr(chunk)) continue;
                    dbg_chunks_ok++;

                    const uintptr_t ent = Memory::Read<uintptr_t>(chunk + (i & 0x1FF) * 0x78);
                    if (!ValidPtr(ent) || ent == local_pawn) continue;
                    dbg_ent_found++;

                    const int hp = Memory::Read<int>(ent + Schema::m_iHealth);
                    if (hp <= 0 || hp > 100) continue;
                    dbg_hp_ok++;

                    const int team = Memory::Read<uint8_t>(ent + Schema::m_iTeamNum);
                    if (team != 2 && team != 3) continue;
                    dbg_team_ok++;

                    if (Memory::Read<uint8_t>(ent + Schema::m_lifeState) != 0) continue;

                    const uintptr_t node = Memory::Read<uintptr_t>(ent + Schema::m_pGameSceneNode);
                    if (!ValidPtr(node)) continue;
                    if (Memory::Read<bool>(node + Schema::m_bDormant)) continue;

                    const uintptr_t bone_arr = Memory::Read<uintptr_t>(node + Schema::m_pBoneArray);
                    if (!ValidPtr(bone_arr)) continue;

                    Bones::Joint joints[Bones::MAX]{};
                    MemReadBuf(bone_arr, joints, sizeof(joints));
                    if (!IsFinite(joints[Bones::HEAD].pos)) continue;

                    Player p{};
                    p.pawn = ent; p.index = i; p.team = team; p.health = hp;
                    p.armor = std::clamp(Memory::Read<int>(ent + Schema::m_iPawnArmor), 0, 100);
                    p.origin = Memory::Read<Vec3>(ent + Schema::m_vOldOrigin);
                    if (!IsFinite(p.origin)) continue;

                    for (int b = 0; b < Bones::MAX; ++b) p.bones[b] = joints[b].pos;
                    p.head = joints[Bones::HEAD].pos; p.head.z += 6.f;

                    const Vec3 a = joints[Bones::ANKLE_L].pos;
                    const Vec3 c = joints[Bones::ANKLE_R].pos;
                    const bool va = IsFinite(a), vc = IsFinite(c);
                    if (va && vc)  p.feet = (a.z < c.z) ? a : c;
                    else if (va)   p.feet = a;
                    else if (vc)   p.feet = c;
                    else { p.feet = joints[Bones::PELVIS].pos; p.feet.z -= 34.f; }
                    p.feet.z -= 2.f;

                    const uint32_t h_ctrl = Memory::Read<uint32_t>(ent + Schema::m_hController);
                    if (h_ctrl != 0 && h_ctrl != 0xFFFFFFFF) {
                        const uintptr_t ctrl_chunk = Memory::Read<uintptr_t>(entity_system + 0x10 + ((h_ctrl & 0x7FFF) >> 9) * 8);
                        if (ValidPtr(ctrl_chunk)) {
                            const uintptr_t ctrl = Memory::Read<uintptr_t>(ctrl_chunk + ((h_ctrl & 0x7FFF) & 0x1FF) * 0x78);
                            if (ValidPtr(ctrl)) {
                                MemReadBuf(ctrl + Schema::m_sSanitizedPlayerName, p.name, sizeof(p.name) - 1);
                                Sanitize(p.name, sizeof(p.name));
                                p.controller = ctrl;
                            }
                        }
                    }

                    const uintptr_t ws = Memory::Read<uintptr_t>(ent + Schema::m_pWeaponServices);
                    if (ValidPtr(ws)) {
                        const uint32_t h_active = Memory::Read<uint32_t>(ws + Schema::m_hActiveWeapon);
                        if (h_active != 0 && h_active != 0xFFFFFFFF) {
                            const uintptr_t w_chunk = Memory::Read<uintptr_t>(entity_system + 0x10 + ((h_active & 0x7FFF) >> 9) * 8);
                            if (ValidPtr(w_chunk)) {
                                const uintptr_t w = Memory::Read<uintptr_t>(w_chunk + ((h_active & 0x7FFF) & 0x1FF) * 0x78);
                                if (ValidPtr(w)) {
                                    char d[64]{};
                                    DesignerName(w, d, sizeof(d));
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

                // Log de diagnóstico cada 60 frames
                static int last_log_frame = -60;
                if (dbg_update_count - last_log_frame >= 60) {
                    last_log_frame = dbg_update_count;
                    char log[512];
                    std::snprintf(log, sizeof(log),
                        "[vanta] upd=%d es=0x%llX max=%d chunks=%d ent=%d hp=%d team=%d players=%d lp=0x%llX\n",
                        dbg_update_count, (unsigned long long)dbg_es, dbg_max_index,
                        dbg_chunks_ok, dbg_ent_found, dbg_hp_ok, dbg_team_ok,
                        (int)players.size(), (unsigned long long)local_pawn);
                    OutputDebugStringA(log);
                }
            }
        };

        static ImU32 hp_gradient(int hp) {
            const float t = std::clamp(hp / 100.f, 0.f, 1.f);
            const float r = t < 0.5f ? 1.f : (1.f - t) * 2.f;
            const float g = t < 0.5f ? t * 2.f : 1.f;
            return IM_COL32((int)(r * 255), (int)(g * 255), 50, 255);
        }

        static float read_curtime(uintptr_t client) {
            const uintptr_t gv = Memory::Read<uintptr_t>(client + Offsets::dwGlobalVars);
            if (!ValidPtr(gv)) return 0.f;
            return Memory::Read<float>(gv + 0x2C);
        }

        static void draw_bomb_info(ImDrawList* dl, int W, uintptr_t client) {
            const uintptr_t c4 = Memory::Read<uintptr_t>(client + Offsets::dwPlantedC4);
            if (!ValidPtr(c4) || !Memory::Read<bool>(c4 + Schema::m_bBombTicking)) return;
            const float blow = Memory::Read<float>(c4 + Schema::m_flC4Blow);
            const float remaining = blow - read_curtime(client);
            if (remaining <= 0.f || remaining > 60.f) return;
            const bool critical = remaining < 5.f;
            const float t = (float)ImGui::GetTime();
            const float pulse = 0.5f + 0.5f * std::sin(t * (critical ? 10.f : 2.8f));
            constexpr float CARD_W = 210.f, CARD_H = 50.f;
            const float cx = W * 0.5f, cy = 62.f;
            const ImVec2 p0(cx - CARD_W * 0.5f, cy), p1(cx + CARD_W * 0.5f, cy + CARD_H);
            dl->AddRectFilled(p0, p1, IM_COL32(10, 12, 18, 245), 5.f);
            dl->AddRect(p0, p1,
                critical ? IM_COL32(210, 65, 55, 255) : IM_COL32(38, 140, 242, 255),
                5.f, 0, 1.2f + pulse * (critical ? 1.2f : 0.f));
            char buf[32];
            const int site = Memory::Read<int>(c4 + Schema::m_nBombSite);
            std::snprintf(buf, sizeof(buf), "C4 [%s] %.1fs", site == 1 ? "B" : "A", remaining);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(cx - ts.x * 0.5f, cy + 16.f),
                critical ? IM_COL32(255, 90, 80, 255) : IM_COL32(230, 235, 245, 255), buf);
        }

        static void draw_radar(ImDrawList* dl, int W,
            const std::vector<Player>& players,
            const Vec3& local_origin, int local_team, const Mat4& vm) {
            constexpr float RADAR_SIZE = 260.f, PADDING = 16.f, RANGE = 6500.f;
            if (!IsFinite(local_origin)) return;
            const float fx = -vm.m[0][2];
            const float fy = -vm.m[1][2];
            const float yaw = std::atan2(fy, fx);
            const float rx = W - RADAR_SIZE - PADDING, ry = PADDING;
            const float cx = rx + RADAR_SIZE * 0.5f, cy = ry + RADAR_SIZE * 0.5f;
            const float scale = (RADAR_SIZE * 0.46f) / RANGE;
            dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + RADAR_SIZE, ry + RADAR_SIZE), IM_COL32(12, 14, 18, 255), 8.f);
            dl->AddRect(ImVec2(rx, ry), ImVec2(rx + RADAR_SIZE, ry + RADAR_SIZE), IM_COL32(50, 160, 230, 200), 8.f, 0, 1.8f);
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
                const float px = std::clamp(cx + ddx * scale, rx + 12.f, rx + RADAR_SIZE - 12.f);
                const float py = std::clamp(cy - ddy * scale, ry + 12.f, ry + RADAR_SIZE - 12.f);
                const ImU32 col = (p.team == local_team) ? IM_COL32(70, 220, 140, 255) : IM_COL32(245, 75, 75, 255);
                dl->AddCircleFilled(ImVec2(px, py), 4.5f, col, 16);
            }
            dl->AddText(ImVec2(rx + 12.f, ry + 8.f), IM_COL32(170, 215, 255, 230), "RADAR");
        }

    } // namespace anónimo

    void Render() {
        auto& v = g_cfg.visuals.player;
        auto& wld = g_cfg.visuals.world;
        if (!v.esp_box && !v.esp_name && !v.esp_health && !v.esp_armor &&
            !v.esp_weapon && !v.esp_distance && !v.esp_skeleton &&
            !wld.radar && !wld.bomb_timer) return;

        const uintptr_t client = Memory::GetModuleBase("client.dll");
        if (!client) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;
        ImGuiIO& io = ImGui::GetIO();
        const int W = (int)io.DisplaySize.x, H = (int)io.DisplaySize.y;

        const Mat4 vm = Memory::Read<Mat4>(client + Offsets::dwViewMatrix);
        const uintptr_t local = Memory::Read<uintptr_t>(client + Offsets::dwLocalPlayerPawn);

        static EntityList entities;
        entities.Update(local);

        Vec3 local_origin{};
        if (ValidPtr(local)) local_origin = Memory::Read<Vec3>(local + Schema::m_vOldOrigin);

        const int local_team = entities.local_team;
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

            const bool enemy = (p.team != local_team);
            const ImU32 col = enemy ? IM_COL32(235, 72, 72, 255) : IM_COL32(86, 214, 140, 255);

            if (v.esp_box) {
                dl->AddRect(ImVec2(l - 1.f, top - 1.f), ImVec2(r + 1.f, bot + 1.f), IM_COL32(0, 0, 0, 90), 0.f, 0, 1.f);
                dl->AddRect(ImVec2(l, top), ImVec2(r, bot), col, 0.f, 0, 1.25f);
            }
            if (v.esp_health) {
                const float f = std::clamp(p.health / 100.f, 0.f, 1.f);
                const float bh = h * f;
                dl->AddRectFilled(ImVec2(l - 6.f, top), ImVec2(l - 3.f, bot), IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled(ImVec2(l - 6.f, bot - bh), ImVec2(l - 3.f, bot), hp_gradient(p.health));
            }
            if (v.esp_armor && p.armor > 0) {
                const float f = std::clamp(p.armor / 100.f, 0.f, 1.f);
                const float bh = h * f;
                dl->AddRectFilled(ImVec2(r + 3.f, top), ImVec2(r + 6.f, bot), IM_COL32(0, 0, 0, 150));
                dl->AddRectFilled(ImVec2(r + 3.f, bot - bh), ImVec2(r + 6.f, bot), IM_COL32(80, 160, 255, 230));
            }
            if (v.esp_name && p.name[0]) {
                const ImVec2 sz = ImGui::CalcTextSize(p.name);
                const float tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                const float ty = top - sz.y - 3.f;
                dl->AddText(ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 180), p.name);
                dl->AddText(ImVec2(tx, ty), IM_COL32(245, 245, 245, 255), p.name);
            }
            float below = bot + 3.f;
            if (v.esp_weapon && p.weapon[0]) {
                const ImVec2 sz = ImGui::CalcTextSize(p.weapon);
                const float tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, below + 1.f), IM_COL32(0, 0, 0, 180), p.weapon);
                dl->AddText(ImVec2(tx, below), IM_COL32(215, 215, 215, 255), p.weapon);
                below += sz.y + 1.f;
            }
            if (v.esp_distance) {
                char dbuf[32];
                std::snprintf(dbuf, sizeof(dbuf), "%.1fm", p.distance * 0.0254f);
                const ImVec2 sz = ImGui::CalcTextSize(dbuf);
                const float tx = std::clamp(hs.x - sz.x * 0.5f, 2.f, (float)W - sz.x - 2.f);
                dl->AddText(ImVec2(tx + 1.f, below + 1.f), IM_COL32(0, 0, 0, 180), dbuf);
                dl->AddText(ImVec2(tx, below), IM_COL32(190, 190, 190, 255), dbuf);
            }
            if (v.esp_skeleton) {
                const ImU32 sk = IM_COL32(
                    (int)(v.skel_color[0] * 255), (int)(v.skel_color[1] * 255),
                    (int)(v.skel_color[2] * 255), 235);
                static const int links[][2] = {
                    {Bones::HEAD, Bones::NECK}, {Bones::NECK, Bones::CHEST},
                    {Bones::CHEST, Bones::SPINE_2}, {Bones::SPINE_2, Bones::SPINE_1},
                    {Bones::SPINE_1, Bones::PELVIS},
                    {Bones::CHEST, Bones::CLAV_L}, {Bones::CLAV_L, Bones::ARM_UP_L},
                    {Bones::ARM_UP_L, Bones::ARM_LO_L}, {Bones::ARM_LO_L, Bones::HAND_L},
                    {Bones::CHEST, Bones::CLAV_R}, {Bones::CLAV_R, Bones::ARM_UP_R},
                    {Bones::ARM_UP_R, Bones::ARM_LO_R}, {Bones::ARM_LO_R, Bones::HAND_R},
                    {Bones::PELVIS, Bones::HIP_L}, {Bones::HIP_L, Bones::KNEE_L},
                    {Bones::KNEE_L, Bones::ANKLE_L},
                    {Bones::PELVIS, Bones::HIP_R}, {Bones::HIP_R, Bones::KNEE_R},
                    {Bones::KNEE_R, Bones::ANKLE_R}
                };
                for (const auto& lnk : links) {
                    if (!IsFinite(p.bones[lnk[0]]) || !IsFinite(p.bones[lnk[1]])) continue;
                    Vec2 a, b;
                    if (!ToScreen(p.bones[lnk[0]], a, vm, W, H)) continue;
                    if (!ToScreen(p.bones[lnk[1]], b, vm, W, H)) continue;
                    dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), sk, 1.15f);
                }
            }
        }

        if (wld.radar)   draw_radar(dl, W, entities.players, local_origin, local_team, vm);
        if (wld.bomb_timer) draw_bomb_info(dl, W, client);
    }

} // namespace Features::Visuals::ESP