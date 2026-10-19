// language: C++, file: src/hooks/present.h
#pragma once
#include <Windows.h>
#include <atomic>
#include <d3d11.h>
#include <dxgi.h>

extern bool g_running;
extern std::atomic<bool> g_unload_requested;

namespace Present {
    bool Init();
    void Shutdown();
    void RenderFrame();
    void RequestUnload();

    HWND                 GetGameWindow();
    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();

    extern bool g_menu_open;
}