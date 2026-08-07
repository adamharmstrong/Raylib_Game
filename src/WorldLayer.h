#pragma once

enum class WorldLayer {
    Background,
    Middleground,
    Foreground
};

inline constexpr int WorldLayerCount = 3;

inline constexpr int WorldLayerIndex(WorldLayer layer) {
    switch (layer) {
    case WorldLayer::Background: return 0;
    case WorldLayer::Middleground: return 1;
    case WorldLayer::Foreground: return 2;
    }
    return 1;
}

inline constexpr bool IsPlayerCollisionLayer(WorldLayer layer) {
    return layer == WorldLayer::Middleground;
}
