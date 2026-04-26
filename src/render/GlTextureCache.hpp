#pragma once

// ============================================================================
// GlTextureCache.hpp  —  OpenGL Texture Upload & Cache  (v0.3.0)
// ============================================================================
//
// Manages glTexImage2D uploads for decoded NDS textures. Each texture is
// identified by name and stored as a GLuint handle. The cache owns the GL
// texture objects and deletes them on destruction or explicit eviction.
//
// Usage:
//   GlTextureCache cache;
//   GLuint id = cache.upload(decodedTex);   // idempotent: same name returns same id
//   glBindTexture(GL_TEXTURE_2D, id);
//   // ... draw ...
//   glBindTexture(GL_TEXTURE_2D, 0);
//
// Thread safety: NOT thread-safe. Must be called from the OpenGL thread.
//
// Requires:
//   An active QOpenGLContext (or equivalent) when calling upload / clear.
// ============================================================================

#include <cstdint>
#include <string>
#include <unordered_map>

#include "nitro/NitroTexture.hpp"

// Forward-declare to avoid pulling in <GL/gl.h> from the header.
typedef unsigned int GLuint;

namespace render
{
    class GlTextureCache
    {
    public:
        GlTextureCache()  = default;
        ~GlTextureCache() { clear(); }

        // Non-copyable, movable.
        GlTextureCache(const GlTextureCache&)            = delete;
        GlTextureCache& operator=(const GlTextureCache&) = delete;
        GlTextureCache(GlTextureCache&&)                 = default;
        GlTextureCache& operator=(GlTextureCache&&)      = default;

        // Upload a decoded texture and return its GL texture object ID.
        // If a texture with the same name is already cached, returns the
        // existing ID without re-uploading.
        GLuint upload(const nitro::DecodedTexture& tex);

        // Upload all textures in the list.
        void uploadAll(const std::vector<nitro::DecodedTexture>& textures);

        // Look up a texture by name. Returns 0 if not found.
        GLuint find(const std::string& name) const;

        // Returns true if any textures are loaded.
        bool empty() const { return m_textures.empty(); }

        // Returns the number of uploaded textures.
        std::size_t count() const { return m_textures.size(); }

        // Delete all GL texture objects and clear the cache.
        // Must be called from an active OpenGL context.
        void clear();

    private:
        std::unordered_map<std::string, GLuint> m_textures;
    };

} // namespace render
