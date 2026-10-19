// language: C++, file: style.cpp
// *Tema VANTA — lee el acento y alpha desde g_cfg.ui cada frame.*
// *Compatible con ImGui pre-1.90.5.*

#include "style.h"
#include "config.h"
#include <imgui.h>

namespace Style {

    static ImVec4 s_accent = ImVec4(0.24f, 0.55f, 0.83f, 1.0f);
    static ImVec4 s_accent_dim = ImVec4(0.16f, 0.37f, 0.57f, 1.0f);

    static ImVec4 PresetColor(int p) {
        switch (p) {
        case 0: return ImVec4(0.24f, 0.55f, 0.83f, 1.0f);   // cold blue
        case 1: return ImVec4(0.85f, 0.20f, 0.15f, 1.0f);   // blood red
        case 2: return ImVec4(0.60f, 0.35f, 0.90f, 1.0f);   // violet
        case 3: return ImVec4(0.20f, 0.75f, 0.55f, 1.0f);   // emerald
        case 4: return ImVec4(0.95f, 0.65f, 0.20f, 1.0f);   // amber
        case 5: return ImVec4(0.90f, 0.35f, 0.60f, 1.0f);   // rose
        }
        return ImVec4(0.24f, 0.55f, 0.83f, 1.0f);
    }

    static ImVec4 hex(unsigned int c, float a = 1.0f) {
        return ImVec4(
            ((c >> 16) & 0xFF) / 255.0f,
            ((c >> 8) & 0xFF) / 255.0f,
            (c & 0xFF) / 255.0f,
            a);
    }

    void Apply() {
        if (g_cfg.ui.theme_preset == 6) {
            s_accent = ImVec4(g_cfg.ui.accent_custom[0], g_cfg.ui.accent_custom[1],
                g_cfg.ui.accent_custom[2], 1.0f);
        }
        else {
            s_accent = PresetColor(g_cfg.ui.theme_preset);
        }
        s_accent_dim = ImVec4(s_accent.x * 0.65f, s_accent.y * 0.65f, s_accent.z * 0.65f, 1.0f);

        ImGuiStyle& st = ImGui::GetStyle();
        ImVec4* c = st.Colors;

        const ImVec4 bg_deep = hex(0x0A0E14);
        const ImVec4 bg_panel = hex(0x0F141B);
        const ImVec4 bg_widget = hex(0x161C25);
        const ImVec4 bg_hover = hex(0x1D2530);
        const ImVec4 bg_active = hex(0x252E3B);
        const ImVec4 text = hex(0xC8CCD4);
        const ImVec4 text_dim = hex(0x6B7280);
        const ImVec4 border = hex(0x1A2230);

        float wa = g_cfg.ui.window_alpha;

        c[ImGuiCol_WindowBg] = ImVec4(bg_deep.x, bg_deep.y, bg_deep.z, wa);
        c[ImGuiCol_ChildBg] = ImVec4(bg_panel.x, bg_panel.y, bg_panel.z, 0.45f);
        c[ImGuiCol_PopupBg] = ImVec4(bg_panel.x, bg_panel.y, bg_panel.z, 0.98f);
        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = text_dim;

        c[ImGuiCol_FrameBg] = bg_widget;
        c[ImGuiCol_FrameBgHovered] = bg_hover;
        c[ImGuiCol_FrameBgActive] = bg_active;

        c[ImGuiCol_TitleBg] = bg_panel;
        c[ImGuiCol_TitleBgActive] = bg_panel;
        c[ImGuiCol_TitleBgCollapsed] = bg_panel;
        c[ImGuiCol_MenuBarBg] = bg_panel;

        c[ImGuiCol_ScrollbarBg] = ImVec4(bg_deep.x, bg_deep.y, bg_deep.z, 0.5f);
        c[ImGuiCol_ScrollbarGrab] = bg_widget;
        c[ImGuiCol_ScrollbarGrabHovered] = bg_hover;
        c[ImGuiCol_ScrollbarGrabActive] = s_accent_dim;

        c[ImGuiCol_CheckMark] = s_accent;
        c[ImGuiCol_SliderGrab] = s_accent;
        c[ImGuiCol_SliderGrabActive] = s_accent;

        c[ImGuiCol_Button] = bg_widget;
        c[ImGuiCol_ButtonHovered] = bg_hover;
        c[ImGuiCol_ButtonActive] = s_accent_dim;

        c[ImGuiCol_Header] = bg_widget;
        c[ImGuiCol_HeaderHovered] = bg_hover;
        c[ImGuiCol_HeaderActive] = s_accent_dim;

        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = s_accent_dim;
        c[ImGuiCol_SeparatorActive] = s_accent;

        c[ImGuiCol_ResizeGrip] = bg_widget;
        c[ImGuiCol_ResizeGripHovered] = bg_hover;
        c[ImGuiCol_ResizeGripActive] = s_accent;

        c[ImGuiCol_Tab] = bg_widget;

        c[ImGuiCol_PlotLines] = s_accent;
        c[ImGuiCol_PlotLinesHovered] = s_accent;
        c[ImGuiCol_PlotHistogram] = s_accent;
        c[ImGuiCol_PlotHistogramHovered] = s_accent;
        c[ImGuiCol_TextSelectedBg] = ImVec4(s_accent.x, s_accent.y, s_accent.z, 0.35f);
        c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
        c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
        c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05f, 0.06f, 0.08f, 0.6f);

        st.WindowRounding = 8.0f;
        st.ChildRounding = 6.0f;
        st.FrameRounding = 4.0f;
        st.PopupRounding = 6.0f;
        st.ScrollbarRounding = 4.0f;
        st.GrabRounding = 4.0f;
        st.TabRounding = 4.0f;

        st.WindowBorderSize = 1.0f;
        st.ChildBorderSize = 1.0f;
        st.PopupBorderSize = 1.0f;
        st.FrameBorderSize = 0.0f;
        st.TabBorderSize = 0.0f;

        st.WindowPadding = ImVec2(10, 10);
        st.FramePadding = ImVec2(10, 6);
        st.CellPadding = ImVec2(6, 4);
        st.ItemSpacing = ImVec2(8, 8);
        st.ItemInnerSpacing = ImVec2(6, 4);
        st.ScrollbarSize = 10.0f;
        st.GrabMinSize = 10.0f;

        st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        st.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        st.SelectableTextAlign = ImVec2(0.0f, 0.5f);

        st.AntiAliasedLines = true;
        st.AntiAliasedLinesUseTex = true;
        st.AntiAliasedFill = true;
        st.CurveTessellationTol = 1.25f;
    }

    ImVec4 Accent() { return s_accent; }
    ImVec4 AccentDim() { return s_accent_dim; }

} // namespace Style