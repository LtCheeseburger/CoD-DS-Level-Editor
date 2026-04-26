#pragma once
// =============================================================================
//  NsbmdTextureRenderer.hpp
// =============================================================================

#include "DecodedNsbmdMesh.hpp"
#include "NitroTexture.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

using GLuint = unsigned int;

struct RenderMeshGL
{
    struct VtxData
    {
        float x = 0.f, y = 0.f, z = 0.f;
        float u = 0.f, v = 0.f;
    };

    std::vector<VtxData> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<std::uint16_t> edgeIndices;

    std::uint32_t texAddr = 0;
};

class NsbmdTextureRenderer
{
public:
    NsbmdTextureRenderer();
    ~NsbmdTextureRenderer();

    NsbmdTextureRenderer(const NsbmdTextureRenderer&)            = delete;
    NsbmdTextureRenderer& operator=(const NsbmdTextureRenderer&) = delete;

    void uploadTextures(const std::vector<NitroTexture>& textures);
    void uploadMeshes(const std::vector<DecodedNsbmdMesh>& meshes);
    void render(bool wireframe = true, bool textured = true);
    void releaseGL();

    int  textureCount() const { return static_cast<int>(texAddrToGL_.size()); }
    int  meshCount() const { return static_cast<int>(meshes_.size()); }
    bool hasTexture(std::uint32_t addr) const { return texAddrToGL_.count(addr) > 0; }

private:
    std::unordered_map<std::uint32_t, GLuint> texAddrToGL_;
    std::vector<RenderMeshGL> meshes_;

    GLuint uploadOneTexture(const NitroTexture& tex);
};
