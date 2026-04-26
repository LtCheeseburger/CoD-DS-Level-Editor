#include "GlTextureCache.hpp"

#include "core/Logger.hpp"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <sstream>

// ============================================================================
// GlTextureCache.cpp  —  OpenGL Texture Upload & Cache  (v0.3.0)
// ============================================================================

namespace render
{
    GLuint GlTextureCache::upload(const nitro::DecodedTexture& tex)
    {
        if (!tex.valid || tex.rgba.empty() || tex.width == 0 || tex.height == 0)
        {
            core::Logger::warning("GlTextureCache: skipping invalid texture \"" + tex.name + "\"");
            return 0u;
        }

        // Return cached handle if already uploaded.
        const auto it = m_textures.find(tex.name);
        if (it != m_textures.end())
            return it->second;

        auto* gl = QOpenGLContext::currentContext();
        if (!gl)
        {
            core::Logger::error("GlTextureCache: no current OpenGL context during upload");
            return 0u;
        }
        auto* f = gl->functions();

        // Generate and bind.
        GLuint texId = 0u;
        f->glGenTextures(1, &texId);
        if (texId == 0u)
        {
            core::Logger::error("GlTextureCache: glGenTextures failed for \"" + tex.name + "\"");
            return 0u;
        }

        f->glBindTexture(GL_TEXTURE_2D, texId);

        // Upload RGBA8 data (already V-flipped by TextureDecoder).
        f->glTexImage2D(
            GL_TEXTURE_2D,
            0,                      // mip level
            GL_RGBA,                // internal format
            tex.width,
            tex.height,
            0,                      // border (must be 0)
            GL_RGBA,                // source format
            GL_UNSIGNED_BYTE,       // source type
            tex.rgba.data()
        );

        // Texture parameters suitable for DS content (no mipmaps, nearest-neighbour
        // matches the hardware look; change to GL_LINEAR for smoother display).
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

        f->glBindTexture(GL_TEXTURE_2D, 0u);

        m_textures[tex.name] = texId;

        std::ostringstream ss;
        ss << "GlTextureCache: uploaded \"" << tex.name << "\""
           << " id=" << texId
           << " " << tex.width << "x" << tex.height;
        core::Logger::info(ss.str());

        return texId;
    }

    void GlTextureCache::uploadAll(const std::vector<nitro::DecodedTexture>& textures)
    {
        for (const nitro::DecodedTexture& t : textures)
            upload(t);
    }

    GLuint GlTextureCache::find(const std::string& name) const
    {
        const auto it = m_textures.find(name);
        return it != m_textures.end() ? it->second : 0u;
    }

    void GlTextureCache::clear()
    {
        auto* gl = QOpenGLContext::currentContext();
        if (!gl)
        {
            // Context may already be gone at shutdown — just leak.
            m_textures.clear();
            return;
        }
        auto* f = gl->functions();

        for (const auto& [name, id] : m_textures)
        {
            GLuint tid = id;
            f->glDeleteTextures(1, &tid);
        }

        m_textures.clear();
    }

} // namespace render
