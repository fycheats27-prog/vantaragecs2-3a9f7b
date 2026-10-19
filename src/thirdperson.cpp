// language: C++, file: src/thirdperson.cpp
// *Lógica de third person. Se llama desde hooks.cpp.*

#include "thirdperson.h"
#include "config.h"
#include <cstdint>

namespace Features::Visuals::ThirdPerson {

    static constexpr int k_m_in_thirdperson = 0x5201;
    static constexpr int k_m_thirdperson_dist = 0x5210;

    void Apply(void* pInput) {
        if (!pInput) return;

        if (g_cfg.visuals.player.third_person) {
            *(bool*)((uintptr_t)pInput + k_m_in_thirdperson) = true;
            *(float*)((uintptr_t)pInput + k_m_thirdperson_dist) = g_cfg.visuals.player.third_person_dist;
        }
        else {
            *(bool*)((uintptr_t)pInput + k_m_in_thirdperson) = false;
        }
    }
}