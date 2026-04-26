#include "NsbmdTextureRenderer.hpp"

#include <QOpenGLContext>

#ifdef _WIN32
#  include <windows.h>
#endif
#ifndef GL_LINES
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

        std::printf("[GL] Upload %s -> addr=0x%08X id=%u\n",
            tex.name.c_str(),
            tex.texAddr,
            id);
    }
}

void NsbmdTextureRenderer::render(const std::vector<nitro::DecodedNsbmdMesh>& meshes)
{
    auto* ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return;

    QOpenGLFunctions* f = ctx->functions();

    for (const auto& mesh : meshes)
    {
        GLuint texID = 0;
        std::uint32_t addr = 0;

        auto it = m_texAddrToGL.find(idx);
        if (it != m_texAddrToGL.end())
        {
            texID = it->second;
            std::printf("[GL] bind idx=%u -> textureID=%u\n", idx, texID);
        }
        else
        {
            addr = mesh.vertexTextureAddr[0];
            if (addr == 0)
            {
                for (std::uint32_t candidate : mesh.vertexTextureAddr)
                {
                    if (candidate != 0)
                    {
                        addr = candidate;
                        break;
                    }
                }
            }

            auto it = m_texAddrToGL.find(addr);
            if (it != m_texAddrToGL.end())
            {
                texID = it->second;
            }
            else if (addr != 0)
            {
                std::printf("[WARN] No texture for addr=0x%08X\n", addr);
            }
            glEnd();
        }

        f->glBindTexture(GL_TEXTURE_2D, texID);

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
