// language: C++, file: src/memory.h
// *Pattern scanning + RPM/WPM sobre client.dll. Compartido por el SDK.*

#pragma once
#include <Windows.h>
#include <cstdint>

namespace Memory {

    // Inicializa: resuelve base de client.dll y su tamaño.
    bool Init();

    // Base de un módulo cargado.
    uintptr_t GetModuleBase(const char* module);

    // Pattern scan estilo IDA: "48 8B C4 ? ? 89 40 18"
    uintptr_t FindPattern(const char* module, const char* pattern);

    // Pattern scan con offset resuelto: lee un int32 en el match+N y lo suma.
    // Útil para "48 8B 05 ?? ?? ?? ?? → lea rax, [rip+offset]"
    uintptr_t FindPatternRip(const char* module, const char* pattern, int disp_offset, int instr_len);

    // Lectura/escritura básica
    bool Read(uintptr_t addr, void* buf, size_t size);
    bool Write(uintptr_t addr, const void* buf, size_t size);

    template<typename T>
    inline T Read(uintptr_t addr) {
        T out{};
        Read(addr, &out, sizeof(T));
        return out;
    }

    template<typename T>
    inline bool Write(uintptr_t addr, const T& val) {
        return Write(addr, &val, sizeof(T));
    }

} // namespace Memory