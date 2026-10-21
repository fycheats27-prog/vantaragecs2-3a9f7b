// language: C++, file: menu.cpp
// *Top bar con drag + sidebar con dots indicadores + content con sub-tabs.*
// *Background animado dibujado antes de todo.*

#include "menu.h"
#include "widgets.h"
#include "config.h"
#include "style.h"
#include "background.h"
#include "chams.h"
#include "present.h"
#include "debug.h" 
#include "aimbot.h"
#include <shellapi.h>
#include <imgui.h>

namespace Menu {

    static int g_active_tab = 0;
    static int g_rage_subtab = 0;
    static int g_visuals_subtab = 0;
    static int g_misc_subtab = 0;

    // ═══════════════════════════════════════════════════════════════
    //  TOP BAR
    // ═══════════════════════════════════════════════════════════════

    static bool   s_dragging = false;
    static ImVec2 s_drag_off = ImVec2(0, 0);

    static void TopBar(ImDrawList* dl, ImVec2 wp, ImVec2 ws, float h) {
        // fondo del bar
        dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + h),
            ImGui::GetColorU32(ImVec4(0.04f, 0.05f, 0.07f, 0.88f)),
            8.0f, ImDrawFlags_RoundCornersTop);
        // línea inferior
        dl->AddLine(ImVec2(wp.x, wp.y + h - 1),
            ImVec2(wp.x + ws.x, wp.y + h - 1),
            ImGui::GetColorU32(ImVec4(0.10f, 0.13f, 0.18f, 1.0f)), 1.0f);

        // dot acento
        ImVec4 A = Style::Accent();
        float cy = wp.y + h * 0.5f;
        dl->AddCircleFilled(ImVec2(wp.x + 16, cy), 6.0f,
            ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, 0.20f)), 16);
        dl->AddCircleFilled(ImVec2(wp.x + 16, cy), 3.5f,
            ImGui::GetColorU32(A), 12);

        // logo
        const char* logo = "VANTA";
        ImVec2 ts = ImGui::CalcTextSize(logo);
        dl->AddText(ImVec2(wp.x + 28, wp.y + (h - ts.y) * 0.5f),
            ImGui::GetColorU32(A), logo);

        // versión
        const char* ver = "v4";
        ImVec2 vs = ImGui::CalcTextSize(ver);
        dl->AddText(ImVec2(wp.x + 28 + ts.x + 8, wp.y + (h - ts.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(0.32f, 0.36f, 0.42f, 1.0f)), ver);

        // hint derecha
        const char* hint = "insert · menu";
        ImVec2 hs = ImGui::CalcTextSize(hint);
        dl->AddText(ImVec2(wp.x + ws.x - hs.x - 14, wp.y + (h - hs.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(0.32f, 0.36f, 0.42f, 1.0f)), hint);
    }

    // ═══════════════════════════════════════════════════════════════
    //  SIDEBAR
    // ═══════════════════════════════════════════════════════════════

    struct SidebarEntry {
        const char* label;
        bool        dot_active;
    };

    static void Sidebar() {
        ImGui::BeginChild("##sidebar", ImVec2(160, 0), true);

        ImGui::Spacing();
        ImGui::Spacing();

        SidebarEntry entries[] = {
            { "rage",     g_cfg.rage.aimbot.enabled || g_cfg.rage.exploits.double_tap || g_cfg.rage.exploits.hide_shots },
            { "anti-aim", g_cfg.anti_aim.enabled },
            { "visuals",  g_cfg.visuals.player.esp_box || g_cfg.visuals.player.chams || g_cfg.visuals.world.radar },
            { "misc",     g_cfg.misc.movement.bhop || g_cfg.misc.world.no_flash || g_cfg.misc.exploits.clantag },
            { "debug",    false },
            { "config",   false },
        };
        const int count = IM_ARRAYSIZE(entries);

        for (int i = 0; i < count; i++) {
            bool active = (g_active_tab == i);

            ImVec2 p = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float h = 34.0f;

            ImGui::InvisibleButton(entries[i].label, ImVec2(w, h));
            bool clicked = ImGui::IsItemClicked();
            bool hovered = ImGui::IsItemHovered();
            if (clicked) g_active_tab = i;

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 e(p.x + w, p.y + h);

            ImVec4 A = Style::Accent();

            if (active) {
                dl->AddRectFilled(p, e,
                    ImGui::GetColorU32(ImVec4(0.11f, 0.15f, 0.20f, 0.75f)), 5.0f);
                dl->AddRectFilled(ImVec2(p.x, p.y + 5), ImVec2(p.x + 2.5f, p.y + h - 5),
                    ImGui::GetColorU32(A), 2.0f);
            }
            else if (hovered) {
                dl->AddRectFilled(p, e,
                    ImGui::GetColorU32(ImVec4(0.07f, 0.09f, 0.12f, 0.5f)), 5.0f);
            }

            // dot indicador
            float cx = p.x + 16.0f;
            float cy = p.y + h * 0.5f;
            if (entries[i].dot_active) {
                dl->AddCircleFilled(ImVec2(cx, cy), 6.0f,
                    ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, 0.30f)), 12);
                dl->AddCircleFilled(ImVec2(cx, cy), 3.0f,
                    ImGui::GetColorU32(A), 10);
            }
            else {
                dl->AddCircleFilled(ImVec2(cx, cy), 2.5f,
                    ImGui::GetColorU32(ImVec4(0.18f, 0.21f, 0.26f, 1.0f)), 10);
            }

            // texto
            ImVec4 tc = active ? ImVec4(0.92f, 0.94f, 0.97f, 1.0f)
                : ImVec4(0.55f, 0.58f, 0.63f, 1.0f);
            ImVec2 ts = ImGui::CalcTextSize(entries[i].label);
            dl->AddText(ImVec2(cx + 12, cy - ts.y * 0.5f),
                ImGui::GetColorU32(tc), entries[i].label);

            ImGui::Spacing();
        }

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 30);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.32f, 0.36f, 0.42f, 1.0f));
        ImGui::TextUnformatted("vanta // v4");
        ImGui::PopStyleColor();

        ImGui::EndChild();
    }

    // ═══════════════════════════════════════════════════════════════
    //  SUB-TABS
    // ═══════════════════════════════════════════════════════════════

    static void SubTabs(const char* const labels[], int count, int* current) {
        for (int i = 0; i < count; i++) {
            if (i) ImGui::SameLine(0, 6);
            bool active = (*current == i);
            if (Widgets::ButtonPill(labels[i], active, ImVec2(100, 26)))
                *current = i;
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // ═══════════════════════════════════════════════════════════════
    //  RAGE
    // ═══════════════════════════════════════════════════════════════

    static void RageAimbotTab() {
        auto& a = g_cfg.rage.aimbot;

        // ─────────────────────────────────────────────
        //  GLOBAL
        // ─────────────────────────────────────────────
        Widgets::Section("global");
        Widgets::Toggle("enabled", &a.enabled);
        Widgets::Keybind("hold key", &a.key);
        Widgets::Toggle("toggle mode", &a.toggle_mode);
        Widgets::Toggle("silent aim", &a.silent);
        Widgets::Toggle("target team", &a.target_team);
        Widgets::Toggle("show fov", &a.show_fov);

        // ─────────────────────────────────────────────
        //  SELECTOR DE ARMA
        // ─────────────────────────────────────────────
        Widgets::Section("weapon category");

        const char* weapon_names[] = {
            "Pistol", "Rifle", "Sniper", "SMG", "Shotgun", "Machine Gun", "Other"
        };
        static int s_weapon_idx = 1;

        float pill_w = 72.0f;
        for (int i = 0; i < IM_ARRAYSIZE(weapon_names); ++i) {
            if (i > 0) ImGui::SameLine(0, 6);
            if (Widgets::ButtonPill(weapon_names[i], s_weapon_idx == i, ImVec2(pill_w, 24))) {
                s_weapon_idx = i;
            }
        }
        ImGui::Spacing();

        auto& w = a.weapons[s_weapon_idx];

        // ─────────────────────────────────────────────
        //  AJUSTES DEL ARMA
        // ─────────────────────────────────────────────
        Widgets::Section("targeting");
        Widgets::SliderInt("fov", &w.fov, 0, 180, "%d deg");
        Widgets::SliderInt("max targets", &w.max_targets, 1, 12, "%d");

        Widgets::Section("hitboxes");
        float cw = 100.0f;
        Widgets::MultiToggle("head", &w.hit_head, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("neck", &w.hit_neck, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("chest", &w.hit_chest, cw);
        Widgets::MultiToggle("stomach", &w.hit_stomach, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("pelvis", &w.hit_pelvis, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("legs", &w.hit_legs, cw);

        Widgets::Section("aim tuning");
        Widgets::Toggle("prediction", &w.prediction);
        if (w.prediction) {
            Widgets::SliderFloat("prediction scale", &w.prediction_scale, 0.0f, 2.0f, "%.2fx");
        }
        Widgets::SliderFloat("smooth", &w.smooth, 0.0f, 1.0f, "%.2f");
        Widgets::SliderInt("reaction (ms)", &w.reaction_ms, 0, 250, "%d ms");

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.47f, 0.54f, 1.0f));
        ImGui::Text("editing: %s", weapon_names[s_weapon_idx]);
        ImGui::PopStyleColor();
    }

    static void RageTriggerTab() {
        auto& t = g_cfg.rage.trigger;

        // ─────────────────────────────────────────────
        //  CURRENT WEAPON
        // ─────────────────────────────────────────────
        Widgets::Section("current weapon");
        const char* wn = Features::Rage::Aimbot::GetCurrentWeaponName();
        const char* cat = Features::Rage::Aimbot::GetCurrentCategoryName();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.96f, 0.98f, 1.0f));
        ImGui::Text("%s", wn);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.66f, 1.0f));
        ImGui::Text("[%s]", cat);
        ImGui::PopStyleColor();

        // ─────────────────────────────────────────────
        //  GLOBAL
        // ─────────────────────────────────────────────
        Widgets::Section("global");
        Widgets::Toggle("enabled", &t.enabled);
        Widgets::Keybind("hold key", &t.key);
        Widgets::Toggle("toggle mode", &t.toggle_mode);
        Widgets::Toggle("target team", &t.target_team);

        // ─────────────────────────────────────────────
        //  WEAPON SELECTOR (auto + manual)
        // ─────────────────────────────────────────────
        Widgets::Section("weapon category");
        const char* names[] = { "Pistol", "Rifle", "Sniper", "SMG", "Shotgun", "Machine Gun", "Other" };
        constexpr int kCount = IM_ARRAYSIZE(names);

        static int      s_idx = 1;
        static int      s_auto_cat = -1;
        static uint64_t s_manual_time = 0;

        int detected = 0;
        for (int i = 0; i < kCount; ++i)
            if (strcmp(cat, names[i]) == 0 || (i == 0 && strstr(cat, "pistol")) ||
                (i == 1 && strstr(cat, "rifle")) || (i == 2 && strstr(cat, "sniper")) ||
                (i == 3 && strstr(cat, "smg")) || (i == 4 && strstr(cat, "shotgun")) ||
                (i == 5 && strstr(cat, "mg")) || (i == 6 && strstr(cat, "other")))
            {
                detected = i; break;
            }

        uint64_t now = GetTickCount64();
        bool user_override = (now - s_manual_time) < 2000;

        if (!user_override && detected != s_auto_cat) {
            s_idx = detected;
            s_auto_cat = detected;
        }

        float pill_w = 72.0f;
        for (int i = 0; i < kCount; ++i) {
            if (i > 0) ImGui::SameLine(0, 6);
            if (Widgets::ButtonPill(names[i], s_idx == i, ImVec2(pill_w, 24))) {
                s_idx = i;
                s_manual_time = now;
                s_auto_cat = i;
            }
        }
        ImGui::Spacing();

        auto& w = t.weapons[s_idx];

        // ─────────────────────────────────────────────
        //  TRIGGER
        // ─────────────────────────────────────────────
        Widgets::Section("trigger");
        Widgets::Toggle("enabled (this weapon)", &w.enabled);
        Widgets::SliderFloat("fov", &w.fov, 0.1f, 10.0f, "%.1f deg");
        Widgets::SliderInt("delay", &w.delay_ms, 0, 300, "%d ms");
        Widgets::Toggle("prediction", &w.prediction);

        // ─────────────────────────────────────────────
        //  HITBOXES
        // ─────────────────────────────────────────────
        Widgets::Section("hitboxes");
        float cw = 100.0f;
        Widgets::MultiToggle("head", &w.hit_head, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("neck", &w.hit_neck, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("chest", &w.hit_chest, cw);
        Widgets::MultiToggle("stomach", &w.hit_stomach, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("pelvis", &w.hit_pelvis, cw); ImGui::SameLine(0, 6);
        Widgets::MultiToggle("legs", &w.hit_legs, cw);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.47f, 0.54f, 1.0f));
        ImGui::Text("editing: %s", names[s_idx]);
        ImGui::PopStyleColor();
    }

    static void RageAccuracyTab() {
        Widgets::Section("hitscan");
        Widgets::SliderFloat("hit chance", &g_cfg.rage.accuracy.hit_chance, 0.0f, 100.0f, "%.0f%%");
        Widgets::SliderFloat("min damage", &g_cfg.rage.accuracy.min_damage, 0.0f, 130.0f, "%.0f");
        Widgets::SliderFloat("penetration", &g_cfg.rage.accuracy.penetration, 0.0f, 100.0f, "%.0f");

        Widgets::Section("preferences");
        Widgets::Toggle("prefer body aim", &g_cfg.rage.accuracy.prefer_body);
        Widgets::Toggle("prefer safe point", &g_cfg.rage.accuracy.prefer_safe);

        Widgets::Section("multipoint");
        Widgets::Toggle("enabled", &g_cfg.rage.accuracy.multipoint);
        if (g_cfg.rage.accuracy.multipoint)
            Widgets::SliderFloat("scale", &g_cfg.rage.accuracy.mp_scale, 0.0f, 100.0f, "%.0f%%");

        Widgets::Section("backtrack");
        Widgets::Toggle("enabled", &g_cfg.rage.accuracy.backtrack);
        if (g_cfg.rage.accuracy.backtrack)
            Widgets::SliderInt("ticks", &g_cfg.rage.accuracy.backtrack_ticks, 1, 16, "%d");
    }

    static void RageExploitsTab() {
        Widgets::Section("shot exploits");
        Widgets::Toggle("double tap", &g_cfg.rage.exploits.double_tap);
        Widgets::Toggle("hide shots", &g_cfg.rage.exploits.hide_shots);
        Widgets::Toggle("rapid fire", &g_cfg.rage.exploits.rapid_fire);

        Widgets::Section("tickbase");
        const char* tb[] = { "off", "teleport", "charge", "fake lag" };
        Widgets::Combo("mode", &g_cfg.rage.exploits.tickbase, tb, IM_ARRAYSIZE(tb));
        if (g_cfg.rage.exploits.tickbase == 3)
            Widgets::SliderInt("lag (ms)", &g_cfg.rage.exploits.fake_lag_ms, 0, 200, "%d ms");
    }

    static void RageTab() {
        const char* subtabs[] = { "aimbot", "triggerbot", "accuracy", "exploits" };
        SubTabs(subtabs, IM_ARRAYSIZE(subtabs), &g_rage_subtab);
        switch (g_rage_subtab) {
        case 0: RageAimbotTab();   break;
        case 1: RageTriggerTab();  break;
        case 2: RageAccuracyTab(); break;
        case 3: RageExploitsTab(); break;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  ANTI-AIM
    // ═══════════════════════════════════════════════════════════════

    static void AntiAimTab() {
        Widgets::Section("anti-aim");
        Widgets::Toggle("enabled", &g_cfg.anti_aim.enabled);

        const char* modes[] = { "jitter", "spin", "static", "random" };
        const char* pitches[] = { "down", "up", "zero", "fake" };
        const char* sides[] = { "auto", "left", "right" };
        const char* shots[] = { "off", "fake", "reverse" };

        Widgets::Combo("mode", &g_cfg.anti_aim.mode, modes, IM_ARRAYSIZE(modes));
        Widgets::Combo("pitch", &g_cfg.anti_aim.pitch, pitches, IM_ARRAYSIZE(pitches));

        Widgets::Section("yaw");
        Widgets::SliderInt("base", &g_cfg.anti_aim.yaw_base, -180, 180, "%d deg");
        Widgets::SliderInt("add", &g_cfg.anti_aim.yaw_add, -180, 180, "%d deg");

        if (g_cfg.anti_aim.mode == 0 || g_cfg.anti_aim.mode == 3)
            Widgets::SliderInt("jitter range", &g_cfg.anti_aim.jitter_range, 0, 180, "%d deg");
        if (g_cfg.anti_aim.mode == 1)
            Widgets::SliderInt("spin speed", &g_cfg.anti_aim.spin_speed, 1, 90, "%d /tick");

        Widgets::Section("fake yaw");
        Widgets::Toggle("desync", &g_cfg.anti_aim.fake_yaw);
        if (g_cfg.anti_aim.fake_yaw) {
            Widgets::SliderInt("amount", &g_cfg.anti_aim.fake_yaw_amount, 0, 60, "%d deg");
            Widgets::Combo("side", &g_cfg.anti_aim.desync_side, sides, IM_ARRAYSIZE(sides));
        }

        Widgets::Section("on shot");
        Widgets::Combo("behaviour", &g_cfg.anti_aim.on_shot, shots, IM_ARRAYSIZE(shots));
    }

    // ═══════════════════════════════════════════════════════════════
    //  VISUALS
    // ═══════════════════════════════════════════════════════════════

    static void VisualsPlayerTab() {
        auto& p = g_cfg.visuals.player;

        Widgets::Section("player esp");
        Widgets::Toggle("box", &p.esp_box);
        if (p.esp_box) {
            Widgets::ColorEdit("box enemy", p.box_enemy);
            Widgets::ColorEdit("box team", p.box_team);
        }
        Widgets::Toggle("name", &p.esp_name);
        if (p.esp_name) Widgets::ColorEdit("name color", p.name_color);
        Widgets::Toggle("health", &p.esp_health);
        Widgets::Toggle("armor", &p.esp_armor);
        Widgets::Toggle("weapon", &p.esp_weapon);
        if (p.esp_weapon) Widgets::ColorEdit("weapon color", p.weapon_color);
        Widgets::Toggle("distance", &p.esp_distance);
        if (p.esp_distance) Widgets::ColorEdit("distance color", p.distance_color);
        Widgets::Toggle("skeleton", &p.esp_skeleton);
        if (p.esp_skeleton) Widgets::ColorEdit("skeleton color", p.skel_color);

        Widgets::Section("glow");
        Widgets::Toggle("enabled", &p.glow);
        if (p.glow) {
            Widgets::ColorEdit("glow enemy", p.glow_enemy);
            Widgets::ColorEdit("glow team", p.glow_team);
            Widgets::Toggle("glow weapons", &p.glow_weapons);
            if (p.glow_weapons) Widgets::ColorEdit("weapon glow", p.glow_weapon_color);
            Widgets::Toggle("glow bomb", &p.glow_bomb);
            if (p.glow_bomb) Widgets::ColorEdit("bomb glow", p.glow_bomb_color);
        }

        Widgets::Section("chams");
        Widgets::Toggle("enabled", &p.chams);
        Features::Visuals::Chams::SetEnabled(p.chams);

        if (p.chams) {
            ImGui::TextColored(ImVec4(0.5f, 0.75f, 1.0f, 1.0f), "chams (D3D11)");
            ImGui::Text("draws/frame:   %d", Debug::g_chams.write_count);
            ImGui::Text("painted/frame: %d", Debug::g_chams.good_players);

            static bool through_walls = Features::Visuals::Chams::GetThroughWalls();
            if (Widgets::Toggle("through walls", &through_walls))
                Features::Visuals::Chams::SetThroughWalls(through_walls);

            static const char* modes[] = { "magenta", "red", "blue" };
            ImGui::Text("mode: %s", modes[Features::Visuals::Chams::CurrentMode()]);
            ImGui::SameLine();
            if (Widgets::Button("Cycle##chamsmode"))
                Features::Visuals::Chams::CycleMode();

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.75f, 1.0f, 1.0f), "preset");

            static int imin = (int)Features::Visuals::Chams::GetIndexMin();
            static int imax = (int)Features::Visuals::Chams::GetIndexMax();
            static int stride_filter = (int)Features::Visuals::Chams::GetStrideFilter();
            static int ib_min_kb = (int)(Features::Visuals::Chams::GetIBMin() / 1024);
            static int ib_max_kb = (int)(Features::Visuals::Chams::GetIBMax() / 1024);
            static int inst_min = (int)Features::Visuals::Chams::GetInstanceMin();
            static int inst_max = (int)Features::Visuals::Chams::GetInstanceMax();

            auto apply = [](int i_min, int i_max, int st, int ib_min, int ib_max, int in_min, int in_max) {
                Features::Visuals::Chams::SetIndexRange((unsigned)i_min, (unsigned)i_max);
                Features::Visuals::Chams::SetStrideFilter((unsigned)st);
                Features::Visuals::Chams::SetIBMin((unsigned)ib_min * 1024);
                Features::Visuals::Chams::SetIBMax((unsigned)ib_max * 1024);
                Features::Visuals::Chams::SetInstanceRange((unsigned)in_min, (unsigned)in_max);
                imin = i_min; imax = i_max; stride_filter = st;
                ib_min_kb = ib_min; ib_max_kb = ib_max;
                inst_min = in_min; inst_max = in_max;
                };

            // Cuerpo perfecto (stride 32)
            if (Widgets::Button("body only")) {
                apply(1000, 300000, 32, 0, 0, 1, 1);
            }
            ImGui::SameLine();
            // Armas + guantes (ignora stride 32, excluye props)
            if (Widgets::Button("weapons + gloves")) {
                apply(1000, 300000, 254, 2, 0, 1, 1);
            }
            ImGui::SameLine();
            // Todo sin props
            if (Widgets::Button("everything (clean)")) {
                apply(1000, 300000, 0, 2, 0, 1, 1);
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.75f, 1.0f, 1.0f), "filter");

            if (ImGui::SliderInt("idx min", &imin, 0, 300000))
                Features::Visuals::Chams::SetIndexRange((unsigned)imin, (unsigned)imax);
            if (ImGui::SliderInt("idx max", &imax, 0, 300000))
                Features::Visuals::Chams::SetIndexRange((unsigned)imin, (unsigned)imax);

            if (ImGui::SliderInt("inst min", &inst_min, 1, 64))
                Features::Visuals::Chams::SetInstanceRange((unsigned)inst_min, (unsigned)inst_max);
            if (ImGui::SliderInt("inst max", &inst_max, 1, 64))
                Features::Visuals::Chams::SetInstanceRange((unsigned)inst_min, (unsigned)inst_max);

            if (ImGui::SliderInt("stride filter", &stride_filter, 0, 255))
                Features::Visuals::Chams::SetStrideFilter((unsigned)stride_filter);
            ImGui::SameLine();
            ImGui::TextDisabled("32=body, 254=not body");

            if (ImGui::SliderInt("IB min (KB)", &ib_min_kb, 0, 2048))
                Features::Visuals::Chams::SetIBMin((unsigned)ib_min_kb * 1024);

            if (ImGui::SliderInt("IB max (KB)", &ib_max_kb, 0, 2048))
                Features::Visuals::Chams::SetIBMax((unsigned)ib_max_kb * 1024);
        }

        Widgets::Section("camera");
        Widgets::Toggle("third person", &p.third_person);
        if (p.third_person)
            Widgets::SliderFloat("distance", &p.third_person_dist, 50.f, 400.f, "%.0f");
    }

    static void VisualsWorldTab() {
        Widgets::Section("radar");
        Widgets::Toggle("enabled", &g_cfg.visuals.world.radar);
        if (g_cfg.visuals.world.radar) {
            const char* pos[] = { "left", "right", "center" };
            Widgets::Combo("position", &g_cfg.visuals.world.radar_pos, pos, IM_ARRAYSIZE(pos));
            Widgets::SliderInt("zoom", &g_cfg.visuals.world.radar_zoom, 0, 100, "%d%%");
        }

        Widgets::Section("world");
        Widgets::Toggle("dropped weapons", &g_cfg.visuals.world.dropped_weapons);
        Widgets::Toggle("grenade prediction", &g_cfg.visuals.world.grenade_pred);
        Widgets::Toggle("bomb esp", &g_cfg.visuals.world.bomb_esp);
        Widgets::Toggle("bomb timer", &g_cfg.visuals.world.bomb_timer);
        Widgets::Toggle("damage markers", &g_cfg.visuals.world.damage_markers);
        Widgets::Toggle("no scope overlay", &g_cfg.visuals.world.no_scope);
    }

    static void VisualsTab() {
        const char* subtabs[] = { "player", "world" };
        SubTabs(subtabs, IM_ARRAYSIZE(subtabs), &g_visuals_subtab);
        if (g_visuals_subtab == 0) VisualsPlayerTab();
        else                       VisualsWorldTab();
    }

    // ═══════════════════════════════════════════════════════════════
    //  MISC
    // ═══════════════════════════════════════════════════════════════

    static void MiscMovementTab() {
        Widgets::Section("movement");
        Widgets::Toggle("bunny hop", &g_cfg.misc.movement.bhop);
        Widgets::Toggle("auto strafe", &g_cfg.misc.movement.auto_strafe);
        Widgets::Toggle("fast stop", &g_cfg.misc.movement.fast_stop);
    }

    static void MiscWorldTab() {
        Widgets::Section("world");
        Widgets::Toggle("no flash", &g_cfg.misc.world.no_flash);
        Widgets::Toggle("no smoke", &g_cfg.misc.world.no_smoke);
        Widgets::Toggle("reveal ranks", &g_cfg.misc.world.reveal_ranks);
    }

    static void MiscExploitsTab() {
        Widgets::Section("exploits");
        Widgets::Toggle("anti untrusted", &g_cfg.misc.exploits.anti_untrusted);

        Widgets::Toggle("clan tag", &g_cfg.misc.exploits.clantag);
        if (g_cfg.misc.exploits.clantag)
            Widgets::InputText("tag", g_cfg.misc.exploits.clantag_text,
                sizeof(g_cfg.misc.exploits.clantag_text));

        Widgets::Toggle("chat spam", &g_cfg.misc.exploits.chat_spam);
        if (g_cfg.misc.exploits.chat_spam)
            Widgets::InputText("message", g_cfg.misc.exploits.chat_text,
                sizeof(g_cfg.misc.exploits.chat_text));
    }

    static void MiscInputTab() {
        Widgets::Section("input");
        Widgets::Keybind("menu key", &g_cfg.misc.input.menu_key);
        Widgets::Keybind("unload key", &g_cfg.misc.input.unload_key);
    }

    static void MiscTab() {
        const char* subtabs[] = { "movement", "world", "exploits", "input" };
        SubTabs(subtabs, IM_ARRAYSIZE(subtabs), &g_misc_subtab);
        switch (g_misc_subtab) {
        case 0: MiscMovementTab(); break;
        case 1: MiscWorldTab();    break;
        case 2: MiscExploitsTab(); break;
        case 3: MiscInputTab();    break;
        }
    }


    // language: C++, file: src/menu.cpp

    static void DebugTab() {
        auto& b = Debug::g_bhop;
        auto& e = Debug::g_esp;

        Widgets::Section("hook");
        ImGui::Text("alive:        %s", b.hook_alive ? "yes" : "no");
        ImGui::Text("bhop enabled: %s", b.bhop_enabled ? "yes" : "no");
        ImGui::Text("tick:         %d", b.tick);

        Widgets::Section("pointers");
        ImGui::Text("cmd:      %p", (void*)b.cmd);
        ImGui::Text("wrapper:  %p", (void*)b.wrapper);
        ImGui::Text("pawn:     %p", (void*)b.pawn);

        Widgets::Section("local player");
        ImGui::Text("hp:        %d", b.hp);
        ImGui::Text("team:      %d", b.team);
        ImGui::Text("life:      %d", b.life);
        ImGui::Text("flags:     0x%08X", b.flags);
        ImGui::Text("on_ground: %s", b.on_ground ? "yes" : "no");
        ImGui::Text("jump_held: %s", b.jump_held ? "yes" : "no");

        // ── NUEVO: ESP DEBUG ──
        Widgets::Section("esp");
        ImGui::Text("entities found:    %d", e.entities_found);
        ImGui::Text("entities on screen: %d", e.entities_on_screen);
        ImGui::Text("local origin:  %.1f, %.1f, %.1f", e.local_x, e.local_y, e.local_z);
        ImGui::Text("target origin: %.1f, %.1f, %.1f", e.target_x, e.target_y, e.target_z);

        Widgets::Section("wrapper memory");
        ImGui::Text("[00] %016llX", b.wrapper_00);
        ImGui::Text("[08] %016llX", b.wrapper_08);
        ImGui::Text("[10] %016llX", b.wrapper_10);
        ImGui::Text("[18] %016llX", b.wrapper_18);
        ImGui::Text("[20] %016llX", b.wrapper_20);
        ImGui::Text("[28] %016llX", b.wrapper_28);
        ImGui::Text("[30] %016llX", b.wrapper_30);
        ImGui::Text("[38] %016llX", b.wrapper_38);

        ImGui::Spacing();
        if (Widgets::Button("open log folder", ImVec2(-1, 32))) {
            char path[MAX_PATH];
            if (GetTempPathA(MAX_PATH, path)) {
                ShellExecuteA(nullptr, "open", path, nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        ImGui::TextDisabled("log: %TEMP%\\vanta_bhop.log");
    }

    // ═══════════════════════════════════════════════════════════════
    //  CONFIG
    // ═══════════════════════════════════════════════════════════════

    static void ConfigTab() {
        Widgets::Section("config slots");
        static char name_buf[64] = "default";
        Widgets::InputText("name", name_buf, sizeof(name_buf));

        ImGui::Spacing();
        if (Widgets::Button("save", ImVec2(80, 30))) { /* TODO */ }
        ImGui::SameLine(0, 6);
        if (Widgets::Button("load", ImVec2(80, 30))) { /* TODO */ }
        ImGui::SameLine(0, 6);
        if (Widgets::Button("delete", ImVec2(80, 30))) { /* TODO */ }

        // ── interfaz ──
        Widgets::Section("background");

        const char* bgs[] = { "none", "particles", "nebula", "waves", "starfield", "aurora" };
        Widgets::Combo("style", &g_cfg.ui.bg_style, bgs, IM_ARRAYSIZE(bgs));

        if (g_cfg.ui.bg_style != 0) {
            Widgets::SliderFloat("intensity", &g_cfg.ui.bg_intensity, 0.1f, 2.0f, "%.2f");
            Widgets::SliderFloat("speed", &g_cfg.ui.bg_speed, 0.1f, 3.0f, "%.2f");
        }

        Widgets::Section("window");
        Widgets::SliderFloat("opacity", &g_cfg.ui.window_alpha, 0.30f, 1.00f, "%.2f");

        Widgets::Section("theme");
        const char* themes[] = {
            "cold blue", "blood red", "violet",
            "emerald",   "amber",     "rose", "custom"
        };
        Widgets::Combo("preset", &g_cfg.ui.theme_preset, themes, IM_ARRAYSIZE(themes));
        if (g_cfg.ui.theme_preset == 6)
            Widgets::ColorEdit("accent", g_cfg.ui.accent_custom);

        // ── danger zone ──
        Widgets::Section("danger zone");
        if (Widgets::DangerButton("unload cheat", ImVec2(-1, 40))) {
            Present::RequestUnload();
        }
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.47f, 0.54f, 1.0f));
        ImGui::TextWrapped(
            "unhooks present, restores wndproc, exits imGui, "
            "and signals the init thread to terminate.");
        ImGui::PopStyleColor();
    }

    // ═══════════════════════════════════════════════════════════════
    //  DRAW
    // ═══════════════════════════════════════════════════════════════

    static void HandleDrag(ImVec2 wp, ImVec2 ws, float top_h) {
        ImVec2 m = ImGui::GetIO().MousePos;
        bool in_bar = (m.y >= wp.y && m.y <= wp.y + top_h &&
            m.x >= wp.x && m.x <= wp.x + ws.x);

        if (ImGui::IsMouseClicked(0) && in_bar && !s_dragging) {
            s_dragging = true;
            s_drag_off = ImVec2(m.x - wp.x, m.y - wp.y);
        }
        if (s_dragging && ImGui::IsMouseDown(0)) {
            ImGui::SetWindowPos(ImVec2(m.x - s_drag_off.x, m.y - s_drag_off.y));
        }
        if (!ImGui::IsMouseDown(0)) s_dragging = false;
    }

    void Draw() {
        ImGui::SetNextWindowSize(ImVec2(820, 560), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(150, 150), ImGuiCond_FirstUseEver);

        ImGui::Begin("##vanta_root", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        // re-aplica estilo cada frame (lee el acento y alpha del config)
        Style::Apply();

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // ── background ──
        ImVec4 A = Style::Accent();
        Background::Draw(dl, wp, ws, g_cfg.ui.bg_style,
            g_cfg.ui.bg_intensity, g_cfg.ui.bg_speed,
            (float)ImGui::GetTime(),
            ImGui::GetColorU32(A));

        // ── top bar ──
        const float top_h = 32.0f;
        TopBar(dl, wp, ws, top_h);
        HandleDrag(wp, ws, top_h);

        // ── sidebar + content ──
        ImGui::SetCursorPos(ImVec2(0, top_h));
        Sidebar();

        ImGui::SameLine();
        ImGui::BeginChild("##content", ImVec2(0, 0), false);

        const char* titles[] = { "rage", "anti-aim", "visuals", "misc", "debug", "config" };
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.94f, 0.97f, 1.0f));
        ImGui::TextUnformatted(titles[g_active_tab]);
        ImGui::PopStyleColor();

        // sublínea con acento
        ImVec2 lp = ImGui::GetCursorScreenPos();
        ImVec2 tl = ImGui::CalcTextSize(titles[g_active_tab]);
        dl->AddRectFilled(ImVec2(lp.x, lp.y - 4), ImVec2(lp.x + tl.x, lp.y - 2),
            ImGui::GetColorU32(A), 1.0f);

        ImGui::Separator();
        ImGui::Spacing();

        switch (g_active_tab) {
        case 0: RageTab();     break;
        case 1: AntiAimTab();  break;
        case 2: VisualsTab();  break;
        case 3: MiscTab();     break;
        case 4: DebugTab();    break;
        case 5: ConfigTab();   break;
        }

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace Menu
