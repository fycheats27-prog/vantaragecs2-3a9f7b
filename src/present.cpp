// language: C++, file: src/hooks/present.cpp
// *Present hook (DXGI vtable idx 8) + WndProc + ImGui lifecycle.*
// *Incluye el renderizado del ESP y el círculo de FOV.*

#include <MinHook.h>
#include "present.h"
#include "style.h"
#include "menu.h"
#include "esp.h"
#include "aimbot.h"
#include "config.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Present {

    static ID3D11Device* g_device = nullptr;
    static ID3D11DeviceContext* g_context = nullptr;
    static IDXGISwapChain* g_swapchain = nullptr;
    static HWND                  g_hwnd = nullptr;
    static WNDPROC               g_original_wndproc = nullptr;
    static bool                  g_initialized = false;

    bool g_menu_open = false;
    std::atomic<bool> g_unload_requested = false;

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    static PresentFn g_original_present = nullptr;

    static void DrawFovCircle() {
        if (!g_cfg.rage.aimbot.enabled || !g_cfg.rage.aimbot.show_fov) return;
        ImGuiIO& io = ImGui::GetIO();
        float cx = io.DisplaySize.x * 0.5f;
        float cy = io.DisplaySize.y * 0.5f;
        int fov = Features::Rage::Aimbot::GetFov();
        float radius = (fov / 90.0f) * (io.DisplaySize.y * 0.5f);
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(36, 140, 210, 140), 64, 1.5f);
    }

    static LRESULT __stdcall WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        if (g_unload_requested.load(std::memory_order_acquire))
            return CallWindowProcW(g_original_wndproc, hwnd, msg, wparam, lparam);

        if (msg == WM_KEYDOWN && wparam == VK_END) {
            RequestUnload();
            return TRUE;
        }

        if (msg == WM_KEYDOWN && wparam == VK_INSERT) {
            g_menu_open = !g_menu_open;
            if (g_menu_open) {
                ShowCursor(TRUE);
                ClipCursor(nullptr);
            }
            else {
                ShowCursor(FALSE);
            }
            return TRUE;
        }

        if (g_menu_open) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

            switch (msg) {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:  case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:  case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:  case WM_MBUTTONUP:
            case WM_XBUTTONDOWN:  case WM_XBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_KEYDOWN:      case WM_KEYUP:
            case WM_CHAR:
                return TRUE;
            }
        }

        return CallWindowProcW(g_original_wndproc, hwnd, msg, wparam, lparam);
    }

    static HRESULT __stdcall hk_Present(IDXGISwapChain* swapchain, UINT sync, UINT flags) {
        if (g_unload_requested.load(std::memory_order_acquire))
            return g_original_present(swapchain, sync, flags);

        g_swapchain = swapchain;

        if (!g_initialized) {
            if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device)))
                return g_original_present(swapchain, sync, flags);

            g_device->GetImmediateContext(&g_context);

            DXGI_SWAP_CHAIN_DESC desc;
            swapchain->GetDesc(&desc);
            g_hwnd = desc.OutputWindow;

            g_original_wndproc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(WndProc)));

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;

            Style::Apply();

            ImGui_ImplWin32_Init(g_hwnd);
            ImGui_ImplDX11_Init(g_device, g_context);

            g_initialized = true;
        }

        RenderFrame();

        return g_original_present(swapchain, sync, flags);
    }

    void RenderFrame() {
        if (!g_initialized) return;
        if (g_unload_requested.load(std::memory_order_acquire)) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_menu_open)
            Menu::Draw();

        Features::Visuals::ESP::Render();
        DrawFovCircle();

        ImGui::Render();

        ID3D11Texture2D* backbuffer = nullptr;
        if (SUCCEEDED(g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&backbuffer)))) {
            ID3D11RenderTargetView* rtv = nullptr;
            if (SUCCEEDED(g_device->CreateRenderTargetView(backbuffer, nullptr, &rtv))) {
                g_context->OMSetRenderTargets(1, &rtv, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                rtv->Release();
            }
            backbuffer->Release();
        }
    }

    bool Init() {
        g_unload_requested.store(false, std::memory_order_release);

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0, 0,
                           GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
                           L"vanta_dummy", nullptr };
        RegisterClassExW(&wc);
        HWND dummy = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = dummy;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* swapchain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        D3D_FEATURE_LEVEL    level;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &sd, &swapchain,
            &device, &level, &context);

        if (FAILED(hr)) {
            DestroyWindow(dummy);
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        void** vtable = *reinterpret_cast<void***>(swapchain);
        void* present_addr = vtable[8];

        bool ok = MH_CreateHook(present_addr, &hk_Present, reinterpret_cast<LPVOID*>(&g_original_present)) == MH_OK;
        if (ok) MH_EnableHook(present_addr);

        device->Release();
        context->Release();
        swapchain->Release();
        DestroyWindow(dummy);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);

        return ok;
    }

    void Shutdown() {
        if (!g_initialized) return;

        if (g_original_wndproc && g_hwnd) {
            SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(g_original_wndproc));
            g_original_wndproc = nullptr;
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (g_context) { g_context->Release(); g_context = nullptr; }
        if (g_device) { g_device->Release();  g_device = nullptr; }
        g_swapchain = nullptr;

        g_initialized = false;
    }

    void RequestUnload() {
        g_unload_requested.store(true, std::memory_order_release);
        g_running = false;
    }

    HWND                 GetGameWindow() { return g_hwnd; }
    ID3D11Device* GetDevice() { return g_device; }
    ID3D11DeviceContext* GetContext() { return g_context; }

} // namespace Present