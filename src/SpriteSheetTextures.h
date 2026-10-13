#pragma once

#include "raylib.h"
#include <string>
#include <vector>

// Load and release only while the rendering context is alive.
class SpriteSheetTextures {
public:
    void Clear() {
        for (Texture2D texture : textures) if (texture.id != 0) UnloadTexture(texture);
        textures.clear();
    }

    void Load(const std::vector<std::string>& paths) {
        Clear();
        for (const auto& path : paths) {
            Texture2D texture = path.empty() ? Texture2D{} : LoadTexture(path.c_str());
            if (texture.id != 0) SetTextureFilter(texture, TEXTURE_FILTER_POINT);
            textures.push_back(texture);
        }
    }

    Texture2D Get(int index, Texture2D foreground, Texture2D background, Texture2D farBackground) const {
        if (index >= 3 && static_cast<size_t>(index - 3) < textures.size())
            return textures[static_cast<size_t>(index - 3)];
        return index == 2 ? farBackground : index == 1 ? background : foreground;
    }

private:
    std::vector<Texture2D> textures;
};
