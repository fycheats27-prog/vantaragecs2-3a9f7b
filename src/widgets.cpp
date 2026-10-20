// language: C++, file: widgets.cpp
// *Widgets custom con el tema VANTA.*
// *IDs únicos por puntero + acento dinámico desde Style::Accent().*

#include "widgets.h"
#include "style.h"
#include <Windows.h>
#include <cstdio>

namespace Widgets {

    // ── paleta local (no-acento) ──
    static const ImVec4 kText = ImVec4(0.78f, 0.82f, 0.86f, 1.0f);
    static const ImVec4 kTextDim = ImVec4(0.42f, 0.47f, 0.54f, 1.0f);
    static const ImVec4 kBgWid = ImVec4(0.086f, 0.11f, 0.145f, 1.0f);
    static const ImVec4 kBgHover = ImVec4(0.11f, 0.14f, 0.18f, 1.0f);
    static const ImVec4 kBorder = ImVec4(0.10f, 0.13f, 0.18f, 1.0f);

    static const char* KeyName(int vk) {
        static char buf[32];
        if (vk == 0) return "none";
        switch (vk) {
        case VK_LBUTTON:  return "mouse1";
        case VK_RBUTTON:  return "mouse2";
        case VK_MBUTTON:  return "mouse3";
        case VK_XBUTTON1: return "mouse4";
        case VK_XBUTTON2: return "mouse5";
        case VK_SHIFT:    return "shift";
        case VK_CONTROL:  return "ctrl";
        case VK_MENU:     return "alt";
        case VK_SPACE:    return "space";
        case VK_TAB:      return "tab";
        case VK_INSERT:   return "insert";
        case VK_DELETE:   return "delete";
        case VK_HOME:     return "home";
        case VK_END:      return "end";
        case VK_PRIOR:    return "pgup";
        case VK_NEXT:     return "pgdn";
        case VK_CAPITAL:  return "caps";
        }
        if (vk >= '0' && vk <= '9') { sprintf_s(buf, "%c", (char)vk); return buf; }
        if (vk >= 'A' && vk <= 'Z') { sprintf_s(buf, "%c", (char)vk); return buf; }
        if (vk >= VK_F1 && vk <= VK_F12) { sprintf_s(buf, "F%d", vk - VK_F1 + 1); return buf; }
        sprintf_s(buf, "0x%02X", vk);
        return buf;
    }

    // ═══════════════════════════════════════════════════════════════
    //  SECTION
    // ═══════════════════════════════════════════════════════════════

    void Section(const char* label) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float w = ImGui::GetContentRegionAvail().x;

        // degradado sutil: empieza acento, termina borde
        ImVec4 A = Style::Accent();
        ImU32 c1 = ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, 0.55f));
        ImU32 c2 = ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, 0.05f));
        dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.0f),
            c1, c2, c2, c1);

        ImGui::Dummy(ImVec2(0, 4));
    }

    // ═══════════════════════════════════════════════════════════════
    //  TOGGLE — switch deslizante
    // ═══════════════════════════════════════════════════════════════

    bool Toggle(const char* label, bool* v) {
        ImGui::PushID(label);
        ImGui::PushID(v);

        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        float h = 24.0f;

        ImGui::InvisibleButton("##row", ImVec2(w, h));
        bool pressed = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();
        if (pressed) *v = !*v;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 A = Style::Accent();
        ImVec4 AD = Style::AccentDim();

        // label a la izquierda
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImU32 lc = *v ? ImGui::GetColorU32(kText) : ImGui::GetColorU32(kTextDim);
        dl->AddText(ImVec2(p.x, p.y + (h - ts.y) * 0.5f), lc, label);

        // switch a la derecha
        float sw = 38.0f, sh = 20.0f;
        ImVec2 sp(p.x + w - sw, p.y + (h - sh) * 0.5f);
        ImVec2 se(sp.x + sw, sp.y + sh);

        ImU32 track = *v ? ImGui::GetColorU32(A) : ImGui::GetColorU32(kBgWid);
        ImU32 bd = *v ? ImGui::GetColorU32(A) : ImGui::GetColorU32(kBorder);

        // glow exterior cuando activo o hover
        if (*v || (hovered && !*v)) {
            float base_a = *v ? 0.14f : 0.08f;
            for (int i = 4; i >= 1; i--)
                dl->AddRectFilled(ImVec2(sp.x - i * 0.8f, sp.y - i * 0.8f),
                    ImVec2(se.x + i * 0.8f, se.y + i * 0.8f),
                    ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, base_a / i)),
                    (sh + i * 1.6f) * 0.5f);
        }

        dl->AddRectFilled(sp, se, track, sh * 0.5f);

        // highlight superior sutil en el track cuando on
        if (*v) {
            ImVec4 hi(A.x * 1.3f, A.y * 1.3f, A.z * 1.3f, 0.55f);
            dl->AddRectFilled(ImVec2(sp.x + 2, sp.y + 1),
                ImVec2(se.x - 2, sp.y + sh * 0.5f - 1),
                ImGui::GetColorU32(hi), (sh * 0.5f) - 1);
        }

        dl->AddRect(sp, se, bd, sh * 0.5f, 0, 1.0f);

        // knob
        float cx = *v ? se.x - sh * 0.5f : sp.x + sh * 0.5f;
        float cy = sp.y + sh * 0.5f;

        // sombra del knob
        dl->AddCircleFilled(ImVec2(cx, cy + 1.0f), sh * 0.5f - 2.5f,
            IM_COL32(0, 0, 0, 80), 20);
        dl->AddCircleFilled(ImVec2(cx, cy), sh * 0.5f - 3.0f,
            IM_COL32(240, 240, 240, 255), 20);
        // acento en el borde interno del knob cuando on
        if (*v)
            dl->AddCircle(ImVec2(cx, cy), sh * 0.5f - 3.0f,
                ImGui::GetColorU32(ImVec4(AD.x, AD.y, AD.z, 0.7f)), 20, 1.0f);

        ImGui::PopID();
        ImGui::PopID();
        return pressed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  MULTI-TOGGLE
    // ═══════════════════════════════════════════════════════════════

    bool MultiToggle(const char* label, bool* value, float width) {
        ImGui::PushID(label);
        ImGui::PushID(value);

        ImVec2 p = ImGui::GetCursorScreenPos();
        float h = 22.0f;

        ImGui::InvisibleButton("##mt", ImVec2(width, h));
        bool pressed = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();
        if (pressed) *value = !*value;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 e(p.x + width, p.y + h);
        ImVec4 A = Style::Accent();
        ImVec4 AD = Style::AccentDim();

        ImU32 bg = *value ? ImGui::GetColorU32(ImVec4(AD.x, AD.y, AD.z, 0.85f))
            : ImGui::GetColorU32(kBgWid);
        if (hovered && !*value) bg = ImGui::GetColorU32(kBgHover);

        ImU32 bd = *value ? ImGui::GetColorU32(A) : ImGui::GetColorU32(kBorder);
        ImU32 tx = *value ? ImGui::GetColorU32(ImVec4(0.95f, 0.96f, 0.98f, 1.0f))
            : ImGui::GetColorU32(kTextDim);

        // glow cuando activo
        if (*value) {
            for (int i = 3; i >= 1; i--)
                dl->AddRectFilled(ImVec2(p.x - i * 0.5f, p.y - i * 0.5f),
                    ImVec2(e.x + i * 0.5f, e.y + i * 0.5f),
                    ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, 0.10f / i)),
                    4.0f + i);
        }

        dl->AddRectFilled(p, e, bg, 4.0f);
        dl->AddRect(p, e, bd, 4.0f, 0, 1.0f);

        ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(p.x + (width - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), tx, label);

        ImGui::PopID();
        ImGui::PopID();
        return pressed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  SLIDER INT
    // ═══════════════════════════════════════════════════════════════

    bool SliderInt(const char* label, int* value, int vmin, int vmax, const char* fmt) {
        ImGui::PushID(label);
        ImGui::PushID(value);

        ImGui::TextUnformatted(label);
        char valbuf[32];
        snprintf(valbuf, sizeof(valbuf), fmt, *value);
        float valw = ImGui::CalcTextSize(valbuf).x;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - valw);
        ImGui::PushStyleColor(ImGuiCol_Text, Style::Accent());
        ImGui::TextUnformatted(valbuf);
        ImGui::PopStyleColor();

        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        float h = 8.0f;

        ImGui::InvisibleButton("##track", ImVec2(w, h));
        bool active = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 e(p.x + w, p.y + h);
        ImVec4 A = Style::Accent();

        // track base
        dl->AddRectFilled(p, e, ImGui::GetColorU32(kBgWid), h * 0.5f);
        // borde superior sutil
        dl->AddLine(ImVec2(p.x + h * 0.5f, p.y),
            ImVec2(e.x - h * 0.5f, p.y),
            ImGui::GetColorU32(kBorder), 1.0f);

        float t = (float)(*value - vmin) / (float)(vmax - vmin);
        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;

        // fill con degradado
        if (t > 0.001f) {
            ImVec4 Ac(A.x, A.y, A.z, 1.0f);
            ImVec4 Ad(A.x * 0.7f, A.y * 0.7f, A.z * 0.7f, 1.0f);
            dl->AddRectFilledMultiColor(p,
                ImVec2(p.x + w * t, p.y + h),
                ImGui::GetColorU32(Ad), ImGui::GetColorU32(Ac),
                ImGui::GetColorU32(Ac), ImGui::GetColorU32(Ad));
        }

        float gx = p.x + w * t;
        float gy = p.y + h * 0.5f;

        // glow del grab
        if (active || hovered) {
            float ga = active ? 0.18f : 0.10f;
            for (int i = 4; i >= 1; i--)
                dl->AddCircleFilled(ImVec2(gx, gy), 6.0f + i * 0.9f,
                    ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, ga / i)), 20);
        }

        // grab
        dl->AddCircleFilled(ImVec2(gx, gy + 1), 6.5f, IM_COL32(0, 0, 0, 90), 20);
        dl->AddCircleFilled(ImVec2(gx, gy), 6.5f, IM_COL32(240, 240, 240, 255), 20);
        dl->AddCircle(ImVec2(gx, gy), 6.5f, ImGui::GetColorU32(A), 20, 1.5f);

        bool changed = false;
        if (active) {
            float mx = ImGui::GetIO().MousePos.x;
            float t2 = (mx - p.x) / w;
            t2 = t2 < 0.0f ? 0.0f : t2 > 1.0f ? 1.0f : t2;
            int nv = vmin + (int)((vmax - vmin) * t2 + 0.5f);
            if (nv != *value) { *value = nv; changed = true; }
        }

        ImGui::PopID();
        ImGui::PopID();
        return changed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  SLIDER FLOAT
    // ═══════════════════════════════════════════════════════════════

    bool SliderFloat(const char* label, float* value, float vmin, float vmax, const char* fmt) {
        ImGui::PushID(label);
        ImGui::PushID(value);

        ImGui::TextUnformatted(label);
        char valbuf[32];
        snprintf(valbuf, sizeof(valbuf), fmt, *value);
        float valw = ImGui::CalcTextSize(valbuf).x;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - valw);
        ImGui::PushStyleColor(ImGuiCol_Text, Style::Accent());
        ImGui::TextUnformatted(valbuf);
        ImGui::PopStyleColor();

        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        float h = 8.0f;

        ImGui::InvisibleButton("##track", ImVec2(w, h));
        bool active = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 e(p.x + w, p.y + h);
        ImVec4 A = Style::Accent();

        dl->AddRectFilled(p, e, ImGui::GetColorU32(kBgWid), h * 0.5f);
        dl->AddLine(ImVec2(p.x + h * 0.5f, p.y),
            ImVec2(e.x - h * 0.5f, p.y),
            ImGui::GetColorU32(kBorder), 1.0f);

        float t = (*value - vmin) / (vmax - vmin);
        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;

        if (t > 0.001f) {
            ImVec4 Ac(A.x, A.y, A.z, 1.0f);
            ImVec4 Ad(A.x * 0.7f, A.y * 0.7f, A.z * 0.7f, 1.0f);
            dl->AddRectFilledMultiColor(p,
                ImVec2(p.x + w * t, p.y + h),
                ImGui::GetColorU32(Ad), ImGui::GetColorU32(Ac),
                ImGui::GetColorU32(Ac), ImGui::GetColorU32(Ad));
        }

        float gx = p.x + w * t;
        float gy = p.y + h * 0.5f;

        if (active || hovered) {
            float ga = active ? 0.18f : 0.10f;
            for (int i = 4; i >= 1; i--)
                dl->AddCircleFilled(ImVec2(gx, gy), 6.0f + i * 0.9f,
                    ImGui::GetColorU32(ImVec4(A.x, A.y, A.z, ga / i)), 20);
        }

        dl->AddCircleFilled(ImVec2(gx, gy + 1), 6.5f, IM_COL32(0, 0, 0, 90), 20);
        dl->AddCircleFilled(ImVec2(gx, gy), 6.5f, IM_COL32(240, 240, 240, 255), 20);
        dl->AddCircle(ImVec2(gx, gy), 6.5f, ImGui::GetColorU32(A), 20, 1.5f);

        bool changed = false;
        if (active) {
            float mx = ImGui::GetIO().MousePos.x;
            float t2 = (mx - p.x) / w;
            t2 = t2 < 0.0f ? 0.0f : t2 > 1.0f ? 1.0f : t2;
            float nv = vmin + (vmax - vmin) * t2;
            if (nv != *value) { *value = nv; changed = true; }
        }

        ImGui::PopID();
        ImGui::PopID();
        return changed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  KEYBIND
    // ═══════════════════════════════════════════════════════════════

    bool Keybind(const char* label, int* key) {
        static bool s_waiting = false;
        static int  s_skip_frames = 0;
        static bool s_block_lmb = false;

        ImGui::PushID(label);
        ImGui::PushID(key);

        ImGui::TextUnformatted(label);
        const char* current = s_waiting ? "press key..." : KeyName(*key);

        float box_w = 100.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - box_w);

        ImVec2 p = ImGui::GetCursorScreenPos();
        float h = 22.0f;
        ImGui::InvisibleButton("##kb", ImVec2(box_w, h));
        bool clicked_l = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        bool clicked_r = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        bool hovered = ImGui::IsItemHovered();

        // Click derecho: limpiar
        if (clicked_r) {
            *key = 0;
            s_waiting = false;
            s_block_lmb = false;
        }

        // Click izquierdo: entrar en modo captura (ignorando el click actual)
        if (clicked_l && !s_waiting) {
            s_waiting = true;
            s_skip_frames = 2;
            s_block_lmb = true;
        }

        // Captura
        if (s_waiting) {
            if (s_skip_frames > 0) {
                s_skip_frames--;
                // Si el LMB se soltó durante el skip, desbloquearlo
                if (s_block_lmb && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                    s_block_lmb = false;
            }
            else {
                // Teclas del teclado (excluyendo el ratón)
                for (int vk = 0x08; vk <= 0xFE; ++vk) {
                    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        *key = vk;
                        s_waiting = false;
                        s_block_lmb = false;
                        break;
                    }
                }

                // Botones del ratón (excepto LMB mientras está bloqueado)
                if (s_waiting) {
                    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) { *key = VK_RBUTTON;  s_waiting = false; s_block_lmb = false; }
                    else if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) { *key = VK_MBUTTON; s_waiting = false; s_block_lmb = false; }
                    else if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) { *key = VK_XBUTTON1; s_waiting = false; s_block_lmb = false; }
                    else if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) { *key = VK_XBUTTON2; s_waiting = false; s_block_lmb = false; }
                    else if (!s_block_lmb && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                        *key = VK_LBUTTON; s_waiting = false; s_block_lmb = false;
                    }
                }

                // Si LMB se soltó, desbloquear
                if (s_block_lmb && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                    s_block_lmb = false;

                // Escape cancela
                if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                    s_waiting = false;
                    s_block_lmb = false;
                }
            }
        }

        // Dibujado de la caja
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 e(p.x + box_w, p.y + h);
        ImVec4 A = Style::Accent();

        ImU32 bg = ImGui::GetColorU32(kBgWid);
        if (s_waiting) {
            ImVec4 Aw(A.x, A.y, A.z, 0.18f);
            bg = ImGui::GetColorU32(Aw);
        }
        else if (hovered) {
            bg = ImGui::GetColorU32(kBgHover);
        }
        else if (*key != 0) {
            ImVec4 Asoft(A.x, A.y, A.z, 0.14f);
            bg = ImGui::GetColorU32(Asoft);
        }

        dl->AddRectFilled(p, e, bg, 4.0f);
        ImU32 bd = (s_waiting || *key != 0) ? ImGui::GetColorU32(A) : ImGui::GetColorU32(kBorder);
        dl->AddRect(p, e, bd, 4.0f, 0, s_waiting ? 1.5f : 1.0f);

        ImVec2 ts = ImGui::CalcTextSize(current);
        ImU32 tc = (s_waiting || *key != 0) ? ImGui::GetColorU32(ImVec4(0.95f, 0.96f, 0.98f, 1.0f))
            : ImGui::GetColorU32(kTextDim);
        dl->AddText(ImVec2(p.x + (box_w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), tc, current);

        ImGui::PopID();
        ImGui::PopID();
        return false;
    }

    // ═══════════════════════════════════════════════════════════════
    //  COMBO
    // ═══════════════════════════════════════════════════════════════

    bool Combo(const char* label, int* current, const char* const items[], int count) {
        ImGui::PushID(label);
        ImGui::PushID(current);

        ImGui::TextUnformatted(label);

        float box_w = 140.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - box_w);

        ImVec4 A = Style::Accent();
        ImVec4 AD = Style::AccentDim();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, kBgWid);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kBgHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kBgHover);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.06f, 0.08f, 0.11f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Header, AD);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, AD);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, A);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));

        ImGui::SetNextItemWidth(box_w);
        bool changed = false;
        if (ImGui::BeginCombo("##c", items[*current])) {
            for (int i = 0; i < count; i++) {
                bool selected = (*current == i);
                if (ImGui::Selectable(items[i], selected)) {
                    *current = i;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(7);
        ImGui::PopID();
        ImGui::PopID();
        return changed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  INPUT TEXT
    // ═══════════════════════════════════════════════════════════════

    bool InputText(const char* label, char* buf, size_t buf_size) {
        ImGui::PushID(label);
        ImGui::PushID(buf);

        ImGui::TextUnformatted(label);

        float box_w = 200.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - box_w);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, kBgWid);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kBgHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kBgHover);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));

        ImGui::SetNextItemWidth(box_w);
        bool changed = ImGui::InputText("##it", buf, buf_size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        ImGui::PopID();
        return changed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  COLOR EDIT
    // ═══════════════════════════════════════════════════════════════

    bool ColorEdit(const char* label, float col[4]) {
        ImGui::PushID(label);
        ImGui::PushID(col);

        ImGui::TextUnformatted(label);

        float box_w = 140.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - box_w);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, kBgWid);

        ImGui::SetNextItemWidth(box_w);
        bool changed = ImGui::ColorEdit4("##col", col,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        ImGui::PopID();
        return changed;
    }

    // ═══════════════════════════════════════════════════════════════
    //  BUTTONS
    // ═══════════════════════════════════════════════════════════════

    bool Button(const char* label, const ImVec2& size) {
        ImGui::PushStyleColor(ImGuiCol_Button, kBgWid);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBgHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Style::AccentDim());
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));

        bool pressed = ImGui::Button(label, size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        return pressed;
    }

    bool ButtonPill(const char* label, bool active, const ImVec2& size) {
        ImVec4 A = Style::Accent();
        ImVec4 AD = Style::AccentDim();

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(AD.x, AD.y, AD.z, 0.75f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AD);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, A);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.96f, 0.98f, 1.0f));
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.055f, 0.075f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBgHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kBgHover);
            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 6));

        bool pressed = ImGui::Button(label, size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        return pressed;
    }

    bool DangerButton(const char* label, const ImVec2& size) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.03f, 0.02f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.06f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.05f, 0.03f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));

        bool pressed = ImGui::Button(label, size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        return pressed;
    }

} // namespace Widgets