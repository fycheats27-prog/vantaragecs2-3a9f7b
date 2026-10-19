// language: C++, file: src/createmove.cpp
// *Lógica de bhop. Se llama desde hooks.cpp.*

#include "createmove.h"
#include "bhop.h"
#include "config.h"
#include <cstdint>

namespace CreateMove {
    void Apply(void* pCmd) {
        if (!pCmd) return;
        Features::Misc::Bhop::OnCreateMove(pCmd);
    }
}