// --- silent.h ---
#pragma once

// Silent aim — escritura de view angles solo en el instante del disparo,
// con restore inmediato para que la cámara no se mueva visiblemente.
//
// Exclusión: si silent_aim.enabled y su key está armada, aimbot y triggerbot
// ceden el control de dwViewAngles / IN_ATTACK en ese tick.
namespace Features::Rage::SilentAim {
    void Init();
    void Shutdown();
    void RenderFovOverlay();  // opcional: dibuja el círculo de FOV en present
}