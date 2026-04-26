#pragma once

#include <unordered_map>
#include <vector>

#include <QOpenGLFunctions>

#include "../nitro/NitroTexture.hpp"
#include "GlTextureCache.hpp"

namespace render
{

class NsbmdTextureRenderer
{
public:
    void setTextures(const std::vector<nitro::DecodedTexture>& textures);

    void render(const std::vector<nitro::DecodedNsbmdMesh>& meshes);

private:
    GlTextureCache m_cache;

    std::vector<nitro::DecodedTexture> m_textures;

    // 🔥 NEW: texAddr → GL texture
    std::unordered_map<uint32_t, GLuint> m_texAddrToGL;
};

}