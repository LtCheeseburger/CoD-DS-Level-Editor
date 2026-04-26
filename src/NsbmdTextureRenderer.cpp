// =============================================================================
//  NsbmdTextureRenderer.cpp
//  OpenGL renderer for NSBMD with GX → texture address binding.
//
//  Texture lookup chain:
//   NitroTexture::texAddr  →  texAddrToGL_  →  glBindTexture
//
//  texAddr is set during Tex0Parser::parse():
//      tex.texAddr = (texImageParam & 0x0000FFFF) << 3
//
//  GxDisplayListDecoder stores the same value in vertexTextureAddr per vertex.
//
//  The renderer matches them via the unordered_map<uint32_t, GLuint> texAddrToGL_.
//  This is the ONLY correct way to associate DS GX geometry with DS textures.
// =============================================================================

#include "NsbmdTextureRenderer.hpp"

// OpenGL — include platform header or use forwarded declarations from NsbmdTextureRenderer.hpp
#ifdef _WIN32
#  include <windows.h>
#endif
#ifndef GL_TRIANGLES  // if a stub was pre-included, skip the system GL header
#  ifdef __APPLE__
#    include <OpenGL/gl.h>
#  else
#    include <GL/gl.h>
#  endif
#endif

#include <cassert>
#include <cstdio>

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------
NsbmdTextureRenderer::NsbmdTextureRenderer()  = default;
NsbmdTextureRenderer::~NsbmdTextureRenderer() { releaseGL(); }

// ---------------------------------------------------------------------------
// uploadTextures()
// ---------------------------------------------------------------------------
void NsbmdTextureRenderer::uploadTextures(const std::vector<NitroTexture>& textures)
{
    for (const NitroTexture& tex : textures)
    {
        if (tex.rgba8.empty()) continue;
        GLuint id = uploadOneTexture(tex);
        if (id != 0)
        {
            texAddrToGL_[tex.texAddr] = id;
#ifndef NDEBUG
            std::printf("[NsbmdTextureRenderer] Uploaded tex '%s' addr=0x%05X w=%d h=%d → GL#%u\n",
                        tex.name.c_str(), tex.texAddr, tex.width, tex.height, id);
#endif
        }
    }
}

// ---------------------------------------------------------------------------
// uploadOneTexture()
// ---------------------------------------------------------------------------
GLuint NsbmdTextureRenderer::uploadOneTexture(const NitroTexture& tex)
{
    if (tex.width == 0 || tex.height == 0 || tex.rgba8.empty()) return 0;

    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0) return 0;

    glBindTexture(GL_TEXTURE_2D, id);

    // DS textures tile – use GL_REPEAT
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 tex.width, tex.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 tex.rgba8.data());

    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

// ---------------------------------------------------------------------------
// uploadMeshes()
// ---------------------------------------------------------------------------
void NsbmdTextureRenderer::uploadMeshes(const std::vector<DecodedNsbmdMesh>& meshes)
{
    meshes_.clear();
    meshes_.reserve(meshes.size());

    for (const DecodedNsbmdMesh& m : meshes)
    {
        if (m.vertices.empty()) continue;

        RenderMeshGL rm;
        rm.texAddr = m.representativeTexAddr();

        // Build interleaved vertex data: X Y Z U V
        struct VtxData { float x, y, z, u, v; };
        std::vector<VtxData> vdata;
        vdata.reserve(m.vertices.size());
        for (const NsbmdVertex& v : m.vertices)
            vdata.push_back({v.x, v.y, v.z, v.u, v.v});

        // VBO
        glGenBuffers(1, &rm.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, rm.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vdata.size() * sizeof(VtxData)),
                     vdata.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Index EBO (triangles)
        if (!m.indices.empty())
        {
            rm.indexCount = static_cast<int>(m.indices.size());
            glGenBuffers(1, &rm.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(m.indices.size() * sizeof(uint16_t)),
                         m.indices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        // Edge EBO (wireframe)
        if (!m.edgeIndices.empty())
        {
            rm.edgeCount = static_cast<int>(m.edgeIndices.size());
            glGenBuffers(1, &rm.edgeEbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm.edgeEbo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(m.edgeIndices.size() * sizeof(uint16_t)),
                         m.edgeIndices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        meshes_.push_back(rm);
    }
}

// ---------------------------------------------------------------------------
// render()
// ---------------------------------------------------------------------------
void NsbmdTextureRenderer::render(bool wireframe, bool textured)
{
    // Set up vertex attribute pointers (fixed-function pipeline compatible path)
    // Interleaved: XYZ (float×3) + UV (float×2) = 20 bytes stride
    constexpr GLsizei kStride = static_cast<GLsizei>(5 * sizeof(float));

    if (textured)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    for (const RenderMeshGL& rm : meshes_)
    {
        if (rm.vbo == 0 || rm.ebo == 0) continue;

        // --- Bind texture ---
        if (textured)
        {
            auto it = texAddrToGL_.find(rm.texAddr);
            if (it != texAddrToGL_.end())
                glBindTexture(GL_TEXTURE_2D, it->second);
            else
                glBindTexture(GL_TEXTURE_2D, 0); // no texture → white
        }

        glBindBuffer(GL_ARRAY_BUFFER, rm.vbo);
        glVertexPointer  (3, GL_FLOAT, kStride, reinterpret_cast<const void*>(0));
        glTexCoordPointer(2, GL_FLOAT, kStride, reinterpret_cast<const void*>(3 * sizeof(float)));

        // --- Solid draw ---
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm.ebo);
        glDrawElements(GL_TRIANGLES, rm.indexCount, GL_UNSIGNED_SHORT, nullptr);

        // --- Wireframe overlay ---
        if (wireframe && rm.edgeEbo != 0 && rm.edgeCount > 0)
        {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.1f, 0.9f, 0.2f); // green wireframe

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm.edgeEbo);
            glDrawElements(GL_LINES, rm.edgeCount, GL_UNSIGNED_SHORT, nullptr);

            glColor3f(1.f, 1.f, 1.f);
            if (textured) glEnable(GL_TEXTURE_2D);
        }
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

// ---------------------------------------------------------------------------
// releaseGL()
// ---------------------------------------------------------------------------
void NsbmdTextureRenderer::releaseGL()
{
    for (RenderMeshGL& rm : meshes_)
    {
        if (rm.vbo)     { glDeleteBuffers(1, &rm.vbo);     rm.vbo = 0; }
        if (rm.ebo)     { glDeleteBuffers(1, &rm.ebo);     rm.ebo = 0; }
        if (rm.edgeEbo) { glDeleteBuffers(1, &rm.edgeEbo); rm.edgeEbo = 0; }
    }
    meshes_.clear();

    for (auto& [addr, id] : texAddrToGL_)
        glDeleteTextures(1, &id);
    texAddrToGL_.clear();
}
