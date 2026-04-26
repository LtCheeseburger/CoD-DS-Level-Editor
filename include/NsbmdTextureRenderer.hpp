#pragma once
// =============================================================================
//  NsbmdTextureRenderer.hpp
//  OpenGL renderer for decoded NSBMD models with texture support.
//
//  Pipeline:
//   1. Upload all NitroTexture::rgba8 to GL → stores in texAddrToGL_
//   2. For each mesh draw call: lookup mesh.representativeTexAddr()
//      in texAddrToGL_ and glBindTexture before drawing.
//   3. Wireframe edges rendered with glDrawElements(GL_LINES, ...).
//
//  The renderer DOES NOT own the decoded model or textures; it holds
//  GL handles only.  Call releaseGL() before destruction or context loss.
// =============================================================================

#include "DecodedNsbmdMesh.hpp"
#include "NitroTexture.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

// OpenGL forward types (avoid including GL headers here)
using GLuint = unsigned int;
using GLenum = unsigned int;

// ---------------------------------------------------------------------------
// RenderMeshGL
//  Per-mesh GPU data.
// ---------------------------------------------------------------------------
struct RenderMeshGL
{
    GLuint vbo        = 0; ///< Vertex buffer (interleaved XYZ+UV)
    GLuint ebo        = 0; ///< Index buffer (triangles)
    GLuint edgeEbo    = 0; ///< Index buffer (lines for wireframe)
    int    indexCount = 0;
    int    edgeCount  = 0;
    uint32_t texAddr  = 0; ///< GX address of representative texture
};

// ---------------------------------------------------------------------------
// NsbmdTextureRenderer
// ---------------------------------------------------------------------------
class NsbmdTextureRenderer
{
public:
    NsbmdTextureRenderer();
    ~NsbmdTextureRenderer();

    // Non-copyable
    NsbmdTextureRenderer(const NsbmdTextureRenderer&)            = delete;
    NsbmdTextureRenderer& operator=(const NsbmdTextureRenderer&) = delete;

    // -----------------------------------------------------------------------
    // uploadTextures()
    //   Call once after Tex0Parser::decode().
    //   Uploads every texture's rgba8 to OpenGL and populates texAddrToGL_.
    //   Textures with rgba8.empty() are skipped.
    // -----------------------------------------------------------------------
    void uploadTextures(const std::vector<NitroTexture>& textures);

    // -----------------------------------------------------------------------
    // uploadMeshes()
    //   Call once after GxDisplayListDecoder::decode().
    //   Builds VBOs/EBOs for every mesh.
    // -----------------------------------------------------------------------
    void uploadMeshes(const std::vector<DecodedNsbmdMesh>& meshes);

    // -----------------------------------------------------------------------
    // render()
    //   Draws all meshes.
    //   wireframe=true  → also draws GL_LINES edge overlay.
    //   textured=true   → binds texture per mesh; false → solid white.
    // -----------------------------------------------------------------------
    void render(bool wireframe = true, bool textured = true);

    // -----------------------------------------------------------------------
    // releaseGL()
    //   Deletes all GL objects.  Safe to call multiple times.
    // -----------------------------------------------------------------------
    void releaseGL();

    // Diagnostics
    int  textureCount()    const { return static_cast<int>(texAddrToGL_.size()); }
    int  meshCount()       const { return static_cast<int>(meshes_.size()); }
    bool hasTexture(uint32_t addr) const { return texAddrToGL_.count(addr) > 0; }

private:
    // texAddr → GL texture ID
    std::unordered_map<uint32_t, GLuint> texAddrToGL_;

    // Per-mesh GPU state
    std::vector<RenderMeshGL> meshes_;

    // Helper: upload one texture, return GLuint (or 0 on failure)
    GLuint uploadOneTexture(const NitroTexture& tex);

    // Internal draw helper
    void drawMesh(const RenderMeshGL& mesh, bool textured);
};
