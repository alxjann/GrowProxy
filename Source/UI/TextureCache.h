#pragma once

#include "Core/Types.h"

#include <imgui.h>

#include <unordered_map>

// Growtopia textures (.rttex from the game folder in %LOCALAPPDATA%\Growtopia) uploaded to OpenGL. Each file is
// loaded the first time an item needs it. Must be used on the UI thread, with the window's GL context current.
class TextureCache {
public:
    struct Sprite {
        ImTextureID texture = 0;
        ImVec2 uv0, uv1;
    };

    // The 32x32 cell (cellX, cellY) of a texture sheet; false if the file can't be loaded or the cell is outside it.
    bool GetSprite(const string& texture, int cellX, int cellY, Sprite& out);

private:
    struct Texture {
        unsigned int id = 0; // 0: failed to load, not retried
        int width = 0;
        int height = 0;
    };

    const Texture& Load(const string& name);

    // shortcut: textures are never deleted; they go away with the GL context when the window closes
    std::unordered_map<string, Texture> m_textures;
};
