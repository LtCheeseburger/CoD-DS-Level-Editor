#include "NsbmdTextureRenderer.hpp"

#ifdef _WIN32
#  include <windows.h>
#endif
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

#include <cstdio>

NsbmdTextureRenderer::NsbmdTextureRenderer()  = default;
NsbmdTextureRenderer::~NsbmdTextureRenderer() { releaseGL(); }

void NsbmdTextureRenderer::uploadTextures(const std::vector<NitroTexture>& textures)
{
    for (const NitroTexture& tex : textures)
    {
        if (tex.rgba8.empty()) continue;

        GLuint id = uploadOneTexture(tex);
        if (id == 0) continue;

        texAddrToGL_[tex.texAddr] = id;
    }
}

GLuint NsbmdTextureRenderer::uploadOneTexture(const NitroTexture& tex)
{
    if (tex.width == 0 || tex.height == 0 || tex.rgba8.empty()) return 0;

    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0) return 0;

    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

#ifdef GL_RGBA8
    const GLint internalFmt = GL_RGBA8;
#else
    const GLint internalFmt = GL_RGBA;
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt,
                 static_cast<GLsizei>(tex.width), static_cast<GLsizei>(tex.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.rgba8.data());

    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

void NsbmdTextureRenderer::uploadMeshes(const std::vector<DecodedNsbmdMesh>& meshes)
{
    meshes_.clear();
    meshes_.reserve(meshes.size());

    for (const DecodedNsbmdMesh& m : meshes)
    {
        if (m.vertices.empty() || m.indices.empty()) continue;

        RenderMeshGL rm;
        rm.texAddr = m.representativeTexAddr();

        rm.vertices.reserve(m.vertices.size());
        for (const NsbmdVertex& v : m.vertices)
            rm.vertices.push_back({v.x, v.y, v.z, v.u, v.v});

        rm.indices = m.indices;
        rm.edgeIndices = m.edgeIndices;

        meshes_.push_back(std::move(rm));
    }
}

void NsbmdTextureRenderer::render(bool wireframe, bool textured)
{
    constexpr GLsizei kStride = static_cast<GLsizei>(5 * sizeof(float));

    if (textured) glEnable(GL_TEXTURE_2D);
    else glDisable(GL_TEXTURE_2D);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    for (const RenderMeshGL& rm : meshes_)
    {
        if (rm.vertices.empty() || rm.indices.empty()) continue;

        if (textured)
        {
            auto it = texAddrToGL_.find(rm.texAddr);
            glBindTexture(GL_TEXTURE_2D, it != texAddrToGL_.end() ? it->second : 0);
        }

        const void* base = rm.vertices.data();
        glVertexPointer(3, GL_FLOAT, kStride, base);
        glTexCoordPointer(2, GL_FLOAT, kStride,
                          reinterpret_cast<const unsigned char*>(base) + 3 * sizeof(float));

        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(rm.indices.size()),
                       GL_UNSIGNED_SHORT,
                       rm.indices.data());

        if (wireframe && !rm.edgeIndices.empty())
        {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.1f, 0.9f, 0.2f);

            glDrawElements(GL_LINES,
                           static_cast<GLsizei>(rm.edgeIndices.size()),
                           GL_UNSIGNED_SHORT,
                           rm.edgeIndices.data());

            glColor3f(1.f, 1.f, 1.f);
            if (textured) glEnable(GL_TEXTURE_2D);
        }
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void NsbmdTextureRenderer::releaseGL()
{
    meshes_.clear();

    for (auto& [addr, id] : texAddrToGL_)
        glDeleteTextures(1, &id);
    texAddrToGL_.clear();
}
