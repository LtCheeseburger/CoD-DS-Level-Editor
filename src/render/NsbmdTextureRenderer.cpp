#include "NsbmdTextureRenderer.hpp"

#include <QOpenGLContext>

#ifdef _WIN32
#  include <windows.h>
#endif
#ifndef GL_TRIANGLES
#  ifdef __APPLE__
#    include <OpenGL/gl.h>
#  else
#    include <GL/gl.h>
#  endif
#endif

#include <cstdio>

namespace render
{

void NsbmdTextureRenderer::setTextures(const std::vector<nitro::DecodedTexture>& textures)
{
    m_textures = textures;
    m_texAddrToGL.clear();

    for (const auto& tex : m_textures)
    {
        GLuint id = m_cache.upload(tex);
        if (id == 0u)
            continue;

        m_texAddrToGL[tex.texAddr] = id;
        std::printf("[GL] upload idx=%u -> textureID=%u\n", tex.texAddr, id);
    }
}

void NsbmdTextureRenderer::render(const std::vector<nitro::DecodedNsbmdMesh>& meshes)
{
    auto* ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return;

    QOpenGLFunctions* f = ctx->functions();
    f->glEnable(GL_TEXTURE_2D);

    for (const auto& mesh : meshes)
    {
        GLuint texID = 0;
        const std::uint32_t idx = mesh.vertexTextureAddr.empty() ? 0u : mesh.vertexTextureAddr[0];

        auto it = m_texAddrToGL.find(idx);
        if (it != m_texAddrToGL.end())
        {
            texID = it->second;
            std::printf("[GL] bind idx=%u -> textureID=%u\n", idx, texID);
        }
        else
        {
            std::printf("[GL] bind idx=%u -> textureID=0 (missing)\n", idx);
        }

        f->glBindTexture(GL_TEXTURE_2D, texID);

        if (!mesh.indices.empty() && mesh.vertices.size() == mesh.uvs.size())
        {
            glBegin(GL_TRIANGLES);
            for (std::uint32_t vidx : mesh.indices)
            {
                if (vidx >= mesh.vertices.size())
                    continue;

                const auto& p = mesh.vertices[vidx];
                const auto& uv = mesh.uvs[vidx];
                glTexCoord2f(uv.x(), uv.y());
                glVertex3f(p.x(), p.y(), p.z());
            }
            glEnd();
        }

        // Keep wireframe overlay for debugging.
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINES);
        for (const auto& e : mesh.edges)
        {
            if (e.a >= mesh.vertices.size() || e.b >= mesh.vertices.size())
                continue;

            const auto& a = mesh.vertices[e.a];
            const auto& b = mesh.vertices[e.b];

            glVertex3f(a.x(), a.y(), a.z());
            glVertex3f(b.x(), b.y(), b.z());
        }
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }

    f->glBindTexture(GL_TEXTURE_2D, 0);
    f->glDisable(GL_TEXTURE_2D);
}

} // namespace render
