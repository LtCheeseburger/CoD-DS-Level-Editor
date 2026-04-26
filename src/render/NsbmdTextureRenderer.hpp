#pragma once

#include <unordered_map>
#include <vector>

#include <QOpenGLFunctions>

#include "../nitro/NitroTexture.hpp"
#include "GlTextureCache.hpp"
#include "nitro/NsbmdGeometryDecoder.hpp"

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

    // GX texAddr -> OpenGL texture object
    std::unordered_map<std::uint32_t, GLuint> m_texAddrToGL;
};

} // namespace render
