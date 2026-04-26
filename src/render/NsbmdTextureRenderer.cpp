#include "NsbmdTextureRenderer.hpp"

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

        m_texAddrToGL[tex.texAddr] = id;

        printf("[GL] Upload %s → addr=0x%08X id=%u\n",
            tex.name.c_str(),
            tex.texAddr,
            id);
    }
}

void NsbmdTextureRenderer::render(const std::vector<nitro::DecodedNsbmdMesh>& meshes)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();

    for (const auto& mesh : meshes)
    {
        GLuint texID = 0;

        if (!mesh.vertexTextureAddr.empty())
        {
            uint32_t addr = mesh.vertexTextureAddr[0];

            auto it = m_texAddrToGL.find(addr);
            if (it != m_texAddrToGL.end())
            {
                texID = it->second;
            }
            else
            {
                printf("[WARN] No texture for addr=0x%08X\n", addr);
            }
        }

        f->glBindTexture(GL_TEXTURE_2D, texID);

        // 🔥 KEEP YOUR EXISTING EDGE DRAWING
        glBegin(GL_LINES);

        for (const auto& e : mesh.edges)
        {
            const auto& a = mesh.vertices[e.a];
            const auto& b = mesh.vertices[e.b];

            glVertex3f(a.x(), a.y(), a.z());
            glVertex3f(b.x(), b.y(), b.z());
        }

        glEnd();
    }
}

}