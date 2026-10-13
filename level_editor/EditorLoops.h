#pragma once
#include "Level.h"
#include <algorithm>
#include <cmath>
#include <map>

inline Rectangle EditorLoopBounds(const ButtonPlatformLoop& loop) {
    const float cap = std::min(loop.radius.x, loop.radius.y);
    return {loop.center.x - cap - loop.platformSize.x * 0.5f,
        loop.center.y - loop.radius.y - loop.platformSize.y * 0.5f,
        cap * 2 + loop.platformSize.x, loop.radius.y * 2 + loop.platformSize.y};
}

inline bool HitEditorLoop(const ButtonPlatformLoop& loop, Vector2 point, float tolerance) {
    for (const auto& platform : loop.platforms)
        if (CheckCollisionPointRec(point, platform)) return true;
    if (tolerance <= 0) return false;
    Vector2 previous = GetButtonPlatformLoopPoint(loop, 0);
    for (int sample = 1; sample <= 128; ++sample) {
        const auto next = GetButtonPlatformLoopPoint(loop, sample / 128.0f);
        const Vector2 delta{next.x - previous.x, next.y - previous.y};
        const float lengthSq = delta.x * delta.x + delta.y * delta.y;
        const float t = lengthSq > 0 ? std::clamp(((point.x - previous.x) * delta.x +
            (point.y - previous.y) * delta.y) / lengthSq, 0.0f, 1.0f) : 0;
        const float dx = point.x - previous.x - delta.x * t, dy = point.y - previous.y - delta.y * t;
        if (dx * dx + dy * dy <= tolerance * tolerance) return true;
        previous = next;
    }
    return false;
}

inline void MoveEditorLoop(Level& level, int index, Vector2 delta) {
    auto& loop = level.buttonPlatformLoops[index];
    loop.center.x += delta.x; loop.center.y += delta.y;
    UpdateButtonPlatformLoopPositions(loop);
    UpdatePlatformLoopButtonPositions(level);
}

inline void ResizeEditorLoop(Level& level, int index, Vector2 delta, bool centered) {
    if (delta.x == 0 && delta.y == 0) return;
    auto& loop = level.buttonPlatformLoops[index];
    const auto before = EditorLoopBounds(loop);
    const float cap = std::max(1.0f, std::min(loop.radius.x, loop.radius.y) + delta.x * 0.5f);
    loop.radius.x = cap;
    loop.radius.y = std::max(cap, loop.radius.y + delta.y * 0.5f);
    const auto after = EditorLoopBounds(loop);
    if (!centered) {
        loop.center.x += (after.width - before.width) * 0.5f;
        loop.center.y += (after.height - before.height) * 0.5f;
    }
    UpdateButtonPlatformLoopPositions(loop);
    UpdatePlatformLoopButtonPositions(level);
}

inline int DuplicateEditorLoop(Level& level, int index, Vector2 offset) {
    if (index < 0 || index >= static_cast<int>(level.buttonPlatformLoops.size())) return -1;
    const int duplicate = static_cast<int>(level.buttonPlatformLoops.size());
    auto loop = level.buttonPlatformLoops[index];
    loop.center.x += offset.x; loop.center.y += offset.y;
    const auto riders = level.platformLoopButtonLinks;
    std::map<int, int> copies;
    for (const auto& rider : riders) {
        if (rider.loopIndex != index || rider.buttonIndex < 0 || rider.buttonIndex >= static_cast<int>(level.buttons.size())) continue;
        if (copies.count(rider.buttonIndex)) continue;
        copies[rider.buttonIndex] = static_cast<int>(level.buttons.size());
        level.buttons.push_back(level.buttons[rider.buttonIndex]);
    }
    if (copies.count(loop.buttonIndex)) loop.buttonIndex = copies[loop.buttonIndex];
    UpdateButtonPlatformLoopPositions(loop);
    level.buttonPlatformLoops.push_back(loop);
    for (auto rider : riders) if (rider.loopIndex == index && copies.count(rider.buttonIndex)) {
        rider.loopIndex = duplicate; rider.buttonIndex = copies[rider.buttonIndex];
        level.platformLoopButtonLinks.push_back(rider);
    }
    UpdatePlatformLoopButtonPositions(level);
    return duplicate;
}

inline bool DeleteEditorLoop(Level& level, int index) {
    if (index < 0 || index >= static_cast<int>(level.buttonPlatformLoops.size())) return false;
    auto& riders = level.platformLoopButtonLinks;
    riders.erase(std::remove_if(riders.begin(), riders.end(), [&](const auto& rider) { return rider.loopIndex == index; }), riders.end());
    for (auto& rider : riders) if (rider.loopIndex > index) --rider.loopIndex;
    level.buttonPlatformLoops.erase(level.buttonPlatformLoops.begin() + index);
    UpdatePlatformLoopButtonPositions(level);
    return true;
}
