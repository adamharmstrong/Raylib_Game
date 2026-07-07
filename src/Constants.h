#pragma once

namespace Constants {
    constexpr int ScreenWidth = 1600;
    constexpr int ScreenHeight = 900;

    constexpr float Gravity = 900.0f;
    constexpr float PlayerSpeed = 260.0f;
    constexpr float PlayerGroundAcceleration = 2100.0f;
    constexpr float PlayerGroundDeceleration = 2600.0f;
    constexpr float PlayerAirAcceleration = 1200.0f;
    constexpr float PlayerAirDeceleration = 850.0f;
    constexpr float PlayerJumpVelocity = -430.0f;
    constexpr float PlayerJumpCutMultiplier = 0.45f;
    constexpr float PlayerMaxFallSpeed = 760.0f;
    constexpr float PlayerClimbSpeed = 190.0f;
    constexpr float CoyoteTime = 0.12f;
    constexpr float JumpBufferTime = 0.12f;
}
