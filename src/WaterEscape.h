#pragma once

#include "Level.h"
#include <algorithm>
#include <cmath>

// Runtime state is separate from authored data so restarting restores every timer.
struct WaterEscapeState {
    bool flooding{false};
    float exitSeconds{0.0f};
    std::vector<bool> previousButtons;
    std::vector<float> barrierSeconds;
    std::vector<float> doorTargets;
    std::vector<float> doorSpeeds;
};

inline void UpdateSimpleHingedDoors(Level& level, WaterEscapeState& state, float dt) {
    if (state.previousButtons.size() != level.buttons.size())
        state.previousButtons.assign(level.buttons.size(), false);
    if (state.barrierSeconds.size() != level.trapDoors.size()) {
        state.barrierSeconds.assign(level.trapDoors.size(), 0.0f);
        state.doorTargets.clear();
        state.doorSpeeds.assign(level.trapDoors.size(), 150.0f);
        for (const TrapDoor& door : level.trapDoors) state.doorTargets.push_back(door.angle);
    }
    for (float& seconds : state.barrierSeconds) seconds = std::max(0.0f, seconds - dt);
    const auto justPressed = [&](int index) {
        return index >= 0 && index < static_cast<int>(level.buttons.size()) &&
            level.buttons[index].pressed && !state.previousButtons[index];
    };
    for (ButtonTrapDoorLink& link : level.buttonTrapDoorLinks) {
        if (link.trapDoorIndex < 0 || link.trapDoorIndex >= static_cast<int>(level.trapDoors.size())) continue;
        const int doorIndex = link.trapDoorIndex;
        if (level.trapDoors[doorIndex].kind != HingedDoorKind::SimpleHingedDoor) continue;
        if (justPressed(link.buttonIndex)) {
            state.doorTargets[doorIndex] = link.openAngle;
            state.doorSpeeds[doorIndex] = link.speed;
            state.barrierSeconds[doorIndex] = link.openAngle > 45.0f ? 5.0f : 0.0f;
        }
    }
    for (size_t i = 0; i < level.trapDoors.size(); ++i) {
        if (level.trapDoors[i].kind != HingedDoorKind::SimpleHingedDoor) continue;
        float& angle = level.trapDoors[i].angle;
        const float delta = state.doorTargets[i] - angle;
        angle += std::clamp(delta, -state.doorSpeeds[i] * dt, state.doorSpeeds[i] * dt);
    }
    for (size_t i = 0; i < level.buttons.size(); ++i) state.previousButtons[i] = level.buttons[i].pressed;
}

inline void UpdateWaterEscape(Level& level, WaterEscapeState& state,
    const std::vector<Rectangle>& livingPlayers, float dt) {
    const int exitButton = level.buttonExitLink.buttonIndex;
    const bool releasePressed = exitButton >= 0 && exitButton < static_cast<int>(level.buttons.size()) &&
        level.buttons[exitButton].pressed &&
        (exitButton >= static_cast<int>(state.previousButtons.size()) || !state.previousButtons[exitButton]);
    UpdateSimpleHingedDoors(level, state, dt);
    // Reaching the lower-right ladder requires crossing this bottom corridor strip.
    for (Rectangle player : livingPlayers)
        if (CheckCollisionRecs(player, {760, 770, 808, 80})) state.flooding = true;
    state.exitSeconds = std::max(0.0f, state.exitSeconds - dt);
    if (releasePressed) state.exitSeconds = 6.0f;
    level.buttonExitLink.activated = state.exitSeconds > 0.0f;
}

inline float WaterEscapeLeakMultiplier(const Level& level, const WaterEscapeState& state) {
    float multiplier = 1.0f;
    for (size_t i = 0; i < state.barrierSeconds.size(); ++i)
        if (state.barrierSeconds[i] > 0.0f && level.trapDoors[i].angle > 65.0f) multiplier *= 0.55f;
    return multiplier;
}
