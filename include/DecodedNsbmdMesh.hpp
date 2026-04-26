#pragma once
// =============================================================================
//  DecodedNsbmdMesh.hpp
//  Decoded geometry from a single NSBMD polygon/mesh.
//
//  Vertex layout is interleaved for easy VBO upload:
//      [X f32][Y f32][Z f32][U f32][V f32]   (20 bytes per vertex)
//
//  Edges are stored as pairs of indices for wireframe rendering.
//
//  TEXTURE INTEGRATION
//  --------------------
//  vertexTextureAddr stores the GX hardware texture address that was
//  active when each vertex was emitted.  The renderer uses
//  vertexTextureAddr[0] (first vertex) as the representative address
//  for the whole mesh draw call.
//
//  The mapping is:
//      GX CMD 0x2A (TEXIMAGE_PARAM) → currentTexAddr stored here
//      currentTexAddr → NitroTexture::texAddr lookup → GLuint bind
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>

// One decoded vertex
struct NsbmdVertex
{
    float x = 0.f, y = 0.f, z = 0.f;
    float u = 0.f, v = 0.f;
};

// One decoded mesh (polygon set)
struct DecodedNsbmdMesh
{
    std::string name;

    // Geometry
    std::vector<NsbmdVertex> vertices;  ///< Decoded positions + UVs
    std::vector<uint16_t>    indices;   ///< Triangle indices

    // Wireframe: pairs of vertex indices forming each edge
    std::vector<uint16_t>    edgeIndices; ///< [i0, i1, i2, i3, ...] pairs

    // Texture address per vertex (size == vertices.size())
    // Contains the GX texAddr that was active when the vertex was emitted.
    // 0 means "no texture bound".
    std::vector<uint32_t> vertexTextureAddr;

    // Primitive type hint (for debug/logging)
    uint8_t primitiveType = 0; // 0=tri, 1=quad, 2=tri-strip, 3=quad-strip

    // Returns the representative texture address for this mesh.
    // Uses first valid (non-zero) addr found in vertexTextureAddr.
    uint32_t representativeTexAddr() const
    {
        for (uint32_t addr : vertexTextureAddr)
            if (addr != 0) return addr;
        return 0;
    }
};

// Top-level decoded model
struct DecodedNsbmdModel
{
    std::string name;
    std::vector<DecodedNsbmdMesh> meshes;
};
