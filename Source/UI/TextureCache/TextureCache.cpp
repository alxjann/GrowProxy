#include "UI/TextureCache/TextureCache.h"

#include "Core/Log.h"
#include "Utils/Rttex/Rttex.h"

#include <windows.h>

#include <GL/gl.h>

#include <fstream>
#include <iterator>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace {

    constexpr int CELL = 32; // pixels per tile in a texture sheet

    // Newer textures are downloaded to cache\game, the ones shipped with the game are in game.
    bool ReadGameFile(const string& name, std::vector<uint8>& out) {
        char root[MAX_PATH];
        DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", root, sizeof(root));
        if (length == 0 || length >= sizeof(root))
            return false;

        for (const char* folder : {"\\Growtopia\\cache\\game\\", "\\Growtopia\\game\\"}) {
            std::ifstream file(string(root) + folder + name, std::ios::binary);
            if (file) {
                out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                return true;
            }
        }
        return false;
    }

    unsigned int Upload(const RgbaImage& image) {
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // keep the pixel art sharp
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     image.pixels.data());

        glBindTexture(GL_TEXTURE_2D, previous);
        return id;
    }

}

const TextureCache::Texture& TextureCache::Load(const string& name) {
    auto it = m_textures.find(name);
    if (it != m_textures.end())
        return it->second;

    Texture& texture = m_textures[name];
    std::vector<uint8> file;
    RgbaImage image;
    if (!ReadGameFile(name, file)) {
        Log::Warn("[Textures] %s not found in the Growtopia folder", name.c_str());
    } else if (!DecodeRttex(file, image)) {
        Log::Warn("[Textures] Could not read %s", name.c_str());
    } else {
        texture.id = Upload(image);
        texture.width = image.width;
        texture.height = image.height;
    }
    return texture;
}

bool TextureCache::GetSprite(const string& name, int cellX, int cellY, Sprite& out) {
    if (name.empty())
        return false;

    const Texture& texture = Load(name);
    if (texture.id == 0 || cellX < 0 || cellY < 0 || (cellX + 1) * CELL > texture.width ||
        (cellY + 1) * CELL > texture.height)
        return false;

    // Stay a hair inside the cell: a sample landing exactly on its edge can round into the neighbouring cell,
    // whose outline then shows as a thin line between joined blocks.
    const float inset = 0.02f;
    out.texture = (ImTextureID)texture.id;
    out.uv0 = ImVec2((cellX * CELL + inset) / texture.width, (cellY * CELL + inset) / texture.height);
    out.uv1 = ImVec2(((cellX + 1) * CELL - inset) / texture.width, ((cellY + 1) * CELL - inset) / texture.height);
    return true;
}
