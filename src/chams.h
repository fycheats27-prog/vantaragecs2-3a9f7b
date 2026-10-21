// --- chams.h ---
#pragma once
#include <d3d11.h>

namespace Features::Visuals::Chams {
    bool Init(ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    void OnPresent(ID3D11DeviceContext* context);

    void SetEnabled(bool on);
    bool GetEnabled();

    void CycleMode();
    int  CurrentMode();

    void SetThroughWalls(bool on);
    bool GetThroughWalls();

    void SetInImGuiRender(bool in);

    void     SetIndexRange(unsigned minI, unsigned maxI);
    unsigned GetIndexMin();
    unsigned GetIndexMax();

    void     SetInstanceRange(unsigned mn, unsigned mx);
    unsigned GetInstanceMin();
    unsigned GetInstanceMax();

    void     SetStrideFilter(unsigned stride);
    unsigned GetStrideFilter();

    void     SetIBMin(unsigned bytes);
    unsigned GetIBMin();

    void     SetIBMax(unsigned bytes);
    unsigned GetIBMax();
}