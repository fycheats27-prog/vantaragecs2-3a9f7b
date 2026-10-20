// language: C++, file: src/createmove.cpp
#include "createmove.h"
#include "bhop.h"
#include "aimbot.h"

namespace CreateMove {
    void Apply(void* pCmd) {
        if (!pCmd) return;
        Features::Misc::Bhop::OnCreateMove(pCmd);
    }
}