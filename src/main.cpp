// language: C++, file: src/main.cpp

#include <Windows.h>
#include "hooks.h"
#include "bhop.h"

bool g_running = true;

static DWORD WINAPI InitThread(LPVOID param) {
    HMODULE self = reinterpret_cast<HMODULE>(param);

    HWND hwnd = nullptr;
    while (g_running) {
        hwnd = FindWindowW(L"SDL_app", nullptr);
        if (hwnd && IsWindowVisible(hwnd)) break;
        hwnd = FindWindowW(nullptr, L"Counter-Strike 2");
        if (hwnd && IsWindowVisible(hwnd)) break;
        Sleep(200);
    }
    if (!g_running) return 0;

    Sleep(2000);

    if (!Hooks::Init()) return 0;

    // Si el bhop usa un thread (como en los archivos anteriores), se inicia aquí
    Features::Misc::Bhop::Init();

    while (g_running) Sleep(100);

    Sleep(150);

    Features::Misc::Bhop::Shutdown();
    Hooks::Shutdown();

    (void)self;
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, InitThread, hModule, 0, nullptr);
    }
    return TRUE;
}