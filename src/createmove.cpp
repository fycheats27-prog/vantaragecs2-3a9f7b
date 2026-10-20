// language: C++, file: src/createmove.cpp
#include "createmove.h"
#include "bhop.h"
#include "aimbot.h"
#include "config.h"
#include <cstdint>

namespace CreateMove {
    void Apply(void* pCmd) {
        if (!pCmd) return;

        // Llamada al Aimbot (Ahora coincide con el nombre de la función en aimbot.h)
        Features::Rage::Aimbot::OnCreateMove(pCmd);

        // Lógica de movimiento
        Features::Misc::Bhop::OnCreateMove(pCmd);
    }
}