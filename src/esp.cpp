// language: C++, file: src/esp.cpp
// *Fix: Iteración de 1 a 128 y validación de equipo y GameSceneNode.*

#include "esp.h"
#include "config.h"
#include "sdk.h"
#include "entity.h"
#include "memory.h"
#include "debug.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "imgui.h"

namespace Features::Visuals::ESP {

    // ── Offsets locales (extraídos de tu dump) ──
    static constexpr int m_pGameSceneNode = 0x330;
    static constexpr int m_pBoneArray = 0x1C0;
    static constexpr int m_bDormant = 0x103;
    static constexpr int m_hController = 0x14BC;
    static constexpr int m_sSanitizedPlayerName = 0x878;
    static constexpr int m_vOldOrigin = 0x14A4;
    static constexpr int m_lifeState = 0x354;

    // ── C4 y reglas ──
    static constexpr uintptr_t dwGameRules = 0x25651D0;
    static constexpr uintptr_t dwPlantedC4 = 0x24CB3C0;
    static constexpr uintptr_t dwGlobalVars = 0x2230090;
    static constexpr int m_bBombPlanted = 0x8C7;
    static constexpr int m_bBombTicking = 0x1288;
    static constexpr int m_flC4Blow = 0x12B8;
    static constexpr int m_flTimerLength = 0x12C0;
    static constexpr int m_bBeingDefused = 0x12C4;
    static constexpr int m_flDefuseCountDown = 0x12D8;
    static constexpr int m_nBombSite = 0x128C;
    static constexpr int m_hBombDefuser = 0x12E0;

    // ── Esqueleto ──
    static constexpr int B_PELVIS = 1, B_SPINE = 2, B_NECK = 6, B_HEAD = 7;
    static constexpr int B_L_SH = 8, B_L_EL = 10, B_L_HN = 11;
    static constexpr int B_R_SH = 12, B_R_EL = 14, B_R_HN = 15;
    static constexpr int B_L_HP = 17, B_L_KN = 18, B_L_FT = 19;
    static constexpr int B_R_HP = 20, B_R_KN = 21, B_R_FT = 22;

    static const int k_skeleton[][2] = {
        { B_HEAD, B_NECK }, { B_NECK, B_SPINE }, { B_SPINE, B_PELVIS },
        { B_NECK, B_L_SH }, { B_L_SH, B_L_EL }, { B_L_EL, B_L_HN },
        { B_NECK, B_R_SH }, { B_R_SH, B_R_EL }, { B_R_EL, B_R_HN },
        { B_PELVIS, B_L_HP }, { B_L_HP, B_L_KN }, { B_L_KN, B_L_FT },
        { B_PELVIS, B_R_HP }, { B_R_HP, B_R_KN }, { B_R_KN, B_R_FT },
    };
    static constexpr int k_n_skel = sizeof(k_skeleton) / sizeof(k_skeleton[0]);

    // ── Helpers ──
    inline bool valid_ptr(uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFF; }
    inline Vec3 vsub(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    inline float vlen(const Vec3& v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }
    inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline bool bad3(const Vec3& v) { return !std::isfinite(v.x) || (v.x == 0.f && v.y == 0.f && v.z == 0.f); }

    struct Mat4 { float m[4][4]; };
    struct PlayerInfo {
        uintptr_t pawn = 0;
        uintptr_t controller = 0;
        int index = 0;
        bool is_local = false;
        bool is_dormant = false;
        int health = 0;
        int team = 0;
        uint8_t life_state = 0;
        char name[64] = {};
    };

    static bool w2s(const Vec3& world, Vec3& screen, const Mat4& vm, int w, int h) {
        float cw = vm.m[3][0] * world.x + vm.m[3][1] * world.y + vm.m[3][2] * world.z + vm.m[3][3];
        if (cw < 0.01f) return false;
        float cx = vm.m[0][0] * world.x + vm.m[0][1] * world.y + vm.m[0][2] * world.z + vm.m[0][3];
        float cy = vm.m[1][0] * world.x + vm.m[1][1] * world.y + vm.m[1][2] * world.z + vm.m[1][3];
        float inv = 1.f / cw;
        screen.x = (w * 0.5f) * (cx * inv + 1.0f);
        screen.y = (h * 0.5f) * (1.0f - cy * inv);
        screen.z = 0.f;
        return std::isfinite(screen.x) && std::isfinite(screen.y);
    }

    static Vec3 get_bone(uintptr_t pawn, int idx) {
        uintptr_t node = Memory::Read<uintptr_t>(pawn + m_pGameSceneNode);
        if (!valid_ptr(node)) return { 0, 0, 0 };
        uintptr_t bones = Memory::Read<uintptr_t>(node + m_pBoneArray);
        if (!valid_ptr(bones)) return { 0, 0, 0 };
        return Memory::Read<Vec3>(bones + (uintptr_t)idx * 0x20);
    }

    static ImU32 hp_gradient(int hp) {
        float t = clampf(hp / 100.f, 0.f, 1.f);
        float r = t < 0.5f ? 1.f : (1.f - t) * 2.f;
        float g = t < 0.5f ? t * 2.f : 1.f;
        return IM_COL32((int)(r * 255), (int)(g * 255), 50, 255);
    }

    // ── Iterador Interno Directo Corregido ──
    static std::vector<PlayerInfo> get_players(uintptr_t local_pawn, uintptr_t client) {
        std::vector<PlayerInfo> fresh;
        uintptr_t entity_list = Memory::Read<uintptr_t>(client + SDK::g_off.dwEntityList);
        if (!valid_ptr(entity_list)) return fresh;

        // CS2 usa hasta 128 entidades (64 controladores + 64 pawns). Iteramos hasta 256 por seguridad.
        for (int i = 1; i <= 256; ++i) {
            uintptr_t list_entry = Memory::Read<uintptr_t>(entity_list + 0x8 * (i >> 9) + 0x10);
            if (!valid_ptr(list_entry)) continue;

            uintptr_t pawn = Memory::Read<uintptr_t>(list_entry + 0x78 * (i & 0x1FF));
            if (!valid_ptr(pawn)) continue;

            // 1. Filtrar equipos: solo T (2) y CT (3)
            uint8_t team = Memory::Read<uint8_t>(pawn + Schema::k_m_iTeamNum);
            if (team != 2 && team != 3) continue;

            // 2. Filtrar vida
            int health = Memory::Read<int>(pawn + Schema::k_m_iHealth);
            if (health <= 0 || health > 100) continue;

            // 3. Filtrar estado
            uint8_t life_state = Memory::Read<uint8_t>(pawn + m_lifeState);
            if (life_state != 0) continue;

            // 4. Validar GameSceneNode
            uintptr_t node = Memory::Read<uintptr_t>(pawn + m_pGameSceneNode);
            if (!valid_ptr(node)) continue;

            bool is_dormant = Memory::Read<uint8_t>(node + m_bDormant);
            if (is_dormant) continue;

            // Si pasó todos los filtros, es un jugador
            PlayerInfo p{};
            p.pawn = pawn;
            p.index = i;
            p.is_local = (pawn == local_pawn);
            p.team = team;
            p.health = health;
            p.life_state = life_state;
            p.is_dormant = is_dormant;

            uint32_t h_ctrl = Memory::Read<uint32_t>(pawn + m_hController);
            if (h_ctrl && h_ctrl != 0xFFFFFFFF) {
                p.controller = SDK::GetEntityFromHandle(h_ctrl);
                if (valid_ptr(p.controller)) {
                    Memory::Read(p.controller + m_sSanitizedPlayerName, p.name, sizeof(p.name) - 1);
                }
            }
            fresh.push_back(p);
        }
        return fresh;
    }

    static float read_curtime(uintptr_t client) {
        uintptr_t gv = Memory::Read<uintptr_t>(client + dwGlobalVars);
        if (!valid_ptr(gv)) return 0.f;
        return Memory::Read<float>(gv + 0x2C);
    }

    // ═══════════════════════════════════════════════════════════════
    //  DRAWING
    // ═══════════════════════════════════════════════════════════════
    static void draw_bomb_info(ImDrawList* dl, int W, int H, uintptr_t client) {
        uintptr_t c4 = Memory::Read<uintptr_t>(client + dwPlantedC4);
        if (!valid_ptr(c4) || !Memory::Read<bool>(c4 + m_bBombTicking)) return;

        float blow_time = Memory::Read<float>(c4 + m_flC4Blow);
        float curtime = read_curtime(client);
        float remaining = blow_time - curtime;
        if (remaining <= 0.f || remaining > 60.f) return;

        bool critical = (remaining < 5.f);
        const float t = (float)ImGui::GetTime();
        const float pulse = 0.5f + 0.5f * sinf(t * (critical ? 10.f : 2.8f));

        constexpr float CARD_W = 210.f, CARD_H = 50.f;
        const float cx = W * 0.5f, cy = 62.f;
        ImVec2 p0(cx - CARD_W * 0.5f, cy), p1(cx + CARD_W * 0.5f, cy + CARD_H);

        dl->AddRectFilled(p0, p1, IM_COL32(10, 12, 18, 245), 5.f);
        dl->AddRect(p0, p1, critical ? IM_COL32(210, 65, 55, 255) : IM_COL32(38, 140, 242, 255), 5.f, 0, 1.2f);

        char tbuf[32];
        int site = Memory::Read<int>(c4 + m_nBombSite);
        snprintf(tbuf, sizeof(tbuf), "C4 [%s]  %.1fs", (site == 1 ? "B" : "A"), remaining);
        ImVec2 ts = ImGui::CalcTextSize(tbuf);
        dl->AddText(ImVec2(cx - ts.x * 0.5f, cy + 16.f), critical ? IM_COL32(255, 90, 80, 255) : IM_COL32(230, 235, 245, 255), tbuf);
    }

    static void draw_radar(ImDrawList* dl, int W, int H, const std::vector<PlayerInfo>& players, uintptr_t local_pawn, int local_team, uintptr_t client) {
        constexpr float RADAR_SIZE = 260.f, PADDING = 16.f, RANGE = 6500.f;
        if (!valid_ptr(local_pawn)) return;

        Vec3 local_origin = Memory::Read<Vec3>(local_pawn + m_vOldOrigin);
        if (bad3(local_origin)) return;
        Vec3 local_view = Memory::Read<Vec3>(client + SDK::g_off.dwViewAngles);

        float rx = W - RADAR_SIZE - PADDING;
        float ry = PADDING;
        float cx = rx + RADAR_SIZE * 0.5f;
        float cy = ry + RADAR_SIZE * 0.5f;
        float scale = (RADAR_SIZE * 0.46f) / RANGE;

        dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + RADAR_SIZE, ry + RADAR_SIZE), IM_COL32(12, 14, 18, 255), 8.f);
        dl->AddRect(ImVec2(rx, ry), ImVec2(rx + RADAR_SIZE, ry + RADAR_SIZE), IM_COL32(50, 160, 230, 200), 8.f, 0, 1.8f);

        const float yaw_rad = local_view.y * 3.14159265f / 180.f;
        const float dirx = cosf(yaw_rad), diry = -sinf(yaw_rad);
        dl->AddTriangleFilled(
            ImVec2(cx + dirx * 10.f, cy + diry * 10.f),
            ImVec2(cx - diry * 6.5f - dirx * 5.5f, cy + dirx * 6.5f - diry * 5.5f),
            ImVec2(cx + diry * 6.5f - dirx * 5.5f, cy - dirx * 6.5f - diry * 5.5f),
            IM_COL32(100, 235, 255, 255));

        for (auto& p : players) {
            if (!p.pawn || p.is_local || p.health <= 0) continue;
            Vec3 origin = Memory::Read<Vec3>(p.pawn + m_vOldOrigin);
            if (bad3(origin)) continue;

            float dx = origin.x - local_origin.x;
            float dy = origin.y - local_origin.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > RANGE) continue;

            float px = clampf(cx + dx * scale, rx + 12.f, rx + RADAR_SIZE - 12.f);
            float py = clampf(cy - dy * scale, ry + 12.f, ry + RADAR_SIZE - 12.f);

            ImU32 col = (p.team == local_team) ? IM_COL32(70, 220, 140, 255) : IM_COL32(245, 75, 75, 255);
            dl->AddCircleFilled(ImVec2(px, py), 4.5f, col, 16);
        }
        dl->AddText(ImVec2(rx + 12.f, ry + 8.f), IM_COL32(170, 215, 255, 230), "RADAR");
    }

    // ═══════════════════════════════════════════════════════════════
    //  MAIN RENDER
    // ═══════════════════════════════════════════════════════════════
    void Render() {
        if (!g_cfg.visuals.player.esp_box && !g_cfg.visuals.player.esp_name &&
            !g_cfg.visuals.player.esp_health && !g_cfg.visuals.player.esp_skeleton &&
            !g_cfg.visuals.world.radar && !g_cfg.visuals.world.bomb_timer) return;

        uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
        if (!client) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        ImGuiIO& io = ImGui::GetIO();
        int W = (int)io.DisplaySize.x;
        int H = (int)io.DisplaySize.y;

        Mat4 vm = Memory::Read<Mat4>(client + SDK::g_off.dwViewMatrix);

        uintptr_t local_pawn = Memory::Read<uintptr_t>(client + SDK::g_off.dwLocalPlayerPawn);
        int local_team = valid_ptr(local_pawn) ? Memory::Read<uint8_t>(local_pawn + Schema::k_m_iTeamNum) : 0;
        Vec3 local_origin = valid_ptr(local_pawn) ? Memory::Read<Vec3>(local_pawn + m_vOldOrigin) : Vec3{ 0, 0, 0 };

        Debug::g_esp.entities_found = 0;
        Debug::g_esp.entities_on_screen = 0;
        Debug::g_esp.local_x = local_origin.x;
        Debug::g_esp.local_y = local_origin.y;
        Debug::g_esp.local_z = local_origin.z;

        auto players = get_players(local_pawn, client);

        for (auto& p : players) {
            if (p.is_local) continue;
            if (p.team == local_team) continue;
            // Ya filtramos health, life_state, dormant, etc. en get_players.

            Debug::g_esp.entities_found++; // Ahora contará enemigos reales

            Vec3 feet_w = Memory::Read<Vec3>(p.pawn + m_vOldOrigin);
            if (bad3(feet_w)) continue;

            Vec3 head_w = feet_w; head_w.z += 68.f;
            Vec3 head_s, feet_s;
            if (!w2s(head_w, head_s, vm, W, H) || !w2s(feet_w, feet_s, vm, W, H)) continue;

            Debug::g_esp.entities_on_screen++;
            if (Debug::g_esp.entities_on_screen == 1) {
                Debug::g_esp.target_x = feet_w.x;
                Debug::g_esp.target_y = feet_w.y;
                Debug::g_esp.target_z = feet_w.z;
            }

            float box_h = fabsf(feet_s.y - head_s.y);
            if (box_h < 4.f || box_h > 1800.f) continue;
            float box_w = box_h * 0.38f;

            float l = head_s.x - box_w * 0.5f;
            float r = head_s.x + box_w * 0.5f;
            float t = head_s.y;
            float b = feet_s.y;

            if (g_cfg.visuals.player.esp_box) {
                dl->AddRect(ImVec2(l, t), ImVec2(r, b), IM_COL32(255, 255, 255, 255), 0.f, 0, 1.2f);
                dl->AddRect(ImVec2(l - 1, t - 1), ImVec2(r + 1, b + 1), IM_COL32(0, 0, 0, 80), 0.f, 0, 1.f);
            }

            if (g_cfg.visuals.player.esp_health) {
                float hpf = clampf(p.health / 100.f, 0.f, 1.f);
                float bar_h = (b - t) * hpf;
                dl->AddRectFilled(ImVec2(l - 5.f, t), ImVec2(l - 2.f, b), IM_COL32(0, 0, 0, 140));
                dl->AddRectFilled(ImVec2(l - 5.f, b - bar_h), ImVec2(l - 2.f, b), hp_gradient(p.health));
            }

            if (g_cfg.visuals.player.esp_name && p.name[0]) {
                ImVec2 sz = ImGui::CalcTextSize(p.name);
                dl->AddText(ImVec2(head_s.x - sz.x * 0.5f, t - 14.f), IM_COL32(255, 255, 255, 255), p.name);
            }

            if (g_cfg.visuals.player.esp_skeleton) {
                ImU32 skel = IM_COL32((int)(g_cfg.visuals.player.skel_color[0] * 255), (int)(g_cfg.visuals.player.skel_color[1] * 255), (int)(g_cfg.visuals.player.skel_color[2] * 255), 255);
                for (int i = 0; i < k_n_skel; i++) {
                    Vec3 aw = get_bone(p.pawn, k_skeleton[i][0]);
                    Vec3 bw = get_bone(p.pawn, k_skeleton[i][1]);
                    if (bad3(aw) || bad3(bw)) continue;
                    Vec3 as, bs;
                    if (!w2s(aw, as, vm, W, H) || !w2s(bw, bs, vm, W, H)) continue;
                    dl->AddLine(ImVec2(as.x, as.y), ImVec2(bs.x, bs.y), skel, 1.1f);
                }
            }
        }

        if (g_cfg.visuals.world.radar) {
            draw_radar(dl, W, H, players, local_pawn, local_team, client);
        }
        if (g_cfg.visuals.world.bomb_timer) {
            draw_bomb_info(dl, W, H, client);
        }
    }

} // namespace Features::Visuals::ESP