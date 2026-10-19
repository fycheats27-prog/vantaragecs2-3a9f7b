// language: C++, file: src/memory.cpp

#include "memory.h"
#include <cstring>

namespace Memory {

    static uintptr_t s_client_base = 0;
    static size_t    s_client_size = 0;

    bool Init() {
        HMODULE h = GetModuleHandleA("client.dll");
        if (!h) return false;
        s_client_base = (uintptr_t)h;

        auto* dos = (IMAGE_DOS_HEADER*)h;
        auto* nt = (IMAGE_NT_HEADERS64*)((uint8_t*)h + dos->e_lfanew);
        s_client_size = nt->OptionalHeader.SizeOfImage;
        return true;
    }

    uintptr_t GetModuleBase(const char* module) {
        return (uintptr_t)GetModuleHandleA(module);
    }

    uintptr_t FindPattern(const char* module, const char* pattern) {
        HMODULE h = GetModuleHandleA(module);
        if (!h) return 0;

        uint8_t bytes[256]; bool wild[256]; int len = 0;
        const char* p = pattern;
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
            };

        while (*p && len < 256) {
            if (*p == ' ') { p++; continue; }
            if (*p == '?') {
                bytes[len] = 0; wild[len] = true;
                p++; if (*p == '?') p++;
                if (*p == ' ') p++;
                len++;
                continue;
            }
            int hi = hex(p[0]); int lo = hex(p[1]);
            if (hi < 0 || lo < 0) break;
            bytes[len] = (uint8_t)((hi << 4) | lo);
            wild[len] = false;
            p += 2; len++;
        }
        if (len == 0) return 0;

        auto* base = (uint8_t*)h;
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
        size_t size = nt->OptionalHeader.SizeOfImage;

        for (size_t i = 0; i + len < size; i++) {
            bool ok = true;
            for (int j = 0; j < len; j++) {
                if (!wild[j] && base[i + j] != bytes[j]) { ok = false; break; }
            }
            if (ok) return (uintptr_t)(base + i);
        }
        return 0;
    }

    uintptr_t FindPatternRip(const char* module, const char* pattern, int disp_offset, int instr_len) {
        uintptr_t match = FindPattern(module, pattern);
        if (!match) return 0;
        int32_t disp = *(int32_t*)(match + disp_offset);
        return match + instr_len + disp;
    }

    bool Read(uintptr_t addr, void* buf, size_t size) {
        SIZE_T got = 0;
        return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, buf, size, &got) && got == size;
    }

    bool Write(uintptr_t addr, const void* buf, size_t size) {
        SIZE_T put = 0;
        return WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, buf, size, &put) && put == size;
    }

} // namespace Memory