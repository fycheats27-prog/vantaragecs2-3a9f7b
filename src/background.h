// language: C++, file: background.h
#pragma once
#include <imgui.h>

namespace Background {

    enum Style : int {
        None = 0,
        Particles,
        Nebula,       // reemplaza a HexGrid
        Waves,
        Starfield,
        Aurora,
        Count
    };

    const char* StyleName(int s);

    void Draw(ImDrawList* dl, ImVec2 pos, ImVec2 size,
        int style, float intensity, float speed, float time, ImU32 accent);

} // namespace Background