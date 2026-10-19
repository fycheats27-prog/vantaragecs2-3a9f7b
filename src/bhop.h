// language: C++, file: src/bhop.h
#pragma once

namespace Features::Misc::Bhop {
    void Init();
    void Shutdown();
    void OnCreateMove(void* cmd);   // legacy — ya no se usa, se deja por compatibilidad
}