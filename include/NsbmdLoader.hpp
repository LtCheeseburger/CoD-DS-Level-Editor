#pragma once
// =============================================================================
//  NsbmdLoader.hpp
//  Convenience class that wires together the full pipeline:
//
//      NSBMD bytes
//        │
//        ├─► Tex0Parser::parse()    →  ParsedTex0 (metadata)
//        │       │
//        │       └─► Tex0Parser::decode()  →  NitroTexture::rgba8 filled
//        │
//        ├─► GxDisplayListDecoder::decode()
//        │       → DecodedNsbmdMesh[] with vertexTextureAddr[] set
//        │
//        └─► NsbmdTextureRenderer
//                ├── uploadTextures()  builds texAddrToGL_
//                ├── uploadMeshes()    builds VBOs/EBOs
//                └── render()         draws textured + wireframe
//
//  texAddr flow
//  ------------
//    Tex0Parser reads texImageParam from the TEX0 name table entry.
//    It computes:  NitroTexture::texAddr = (texImageParam & 0xFFFF) << 3
//
//    GxDisplayListDecoder executes CMD 0x2A (TEXIMAGE_PARAM).
//    It extracts:  currentTexAddr_ = (param & 0xFFFF) << 3
//    This is pushed into vertexTextureAddr for every emitVertex() call.
//
//    NsbmdTextureRenderer builds:  texAddrToGL_[texAddr] = glID
//    Before each mesh draw: looks up mesh.representativeTexAddr() → glBindTexture.
//
//  The two computations are IDENTICAL, so the keys always match.
// =============================================================================

#include "DecodedNsbmdMesh.hpp"
#include "GxDisplayListDecoder.hpp"
#include "NsbmdTextureRenderer.hpp"
#include "NitroTexture.hpp"
#include "Tex0Parser.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

class NsbmdLoader
{
public:
    NsbmdLoader() = default;

    // -----------------------------------------------------------------------
    // load()
    //   nsbmdData – full raw NSBMD file bytes.
    //   Returns true on success; renderer is ready to call render().
    // -----------------------------------------------------------------------
    bool load(std::span<const uint8_t> nsbmdData)
    {
        // ---- Locate TEX0 section ----
        // NSBMD file: 16-byte file header, then section headers.
        // Find the TEX0 section by its magic 0x30584554 ("TEX0").
        auto tex0Span = findSection(nsbmdData, 0x30584554u /*'TEX0'*/);

        // ---- Parse + decode textures ----
        Tex0Parser texParser;
        ParsedTex0 parsed;
        if (!tex0Span.empty())
        {
            parsed = texParser.parse(tex0Span);
            if (parsed.valid)
                texParser.decode(parsed, tex0Span);
        }

        // ---- Locate MDL0 display list(s) ----
        // MDL0 magic: 0x304C444D ("MDL0")
        auto mdl0Span = findSection(nsbmdData, 0x304C444Du /*'MDL0'*/);

        // ---- Decode geometry ----
        GxDisplayListDecoder gxDecoder;
        std::vector<DecodedNsbmdMesh> meshes;
        if (!mdl0Span.empty())
        {
            // The display list is embedded after the MDL0 header.
            // The exact offset depends on your MDL0 parser; pass the whole
            // section and the decoder will skip unknown commands safely.
            meshes = gxDecoder.decode(mdl0Span);
        }

        // ---- Upload to GPU ----
        renderer_.releaseGL();
        if (parsed.valid && !parsed.textures.empty())
            renderer_.uploadTextures(parsed.textures);
        if (!meshes.empty())
            renderer_.uploadMeshes(meshes);

        loaded_ = true;
        return loaded_;
    }

    // Call once per frame from your QOpenGLWidget::paintGL()
    void render(bool wireframe = true, bool textured = true)
    {
        if (loaded_)
            renderer_.render(wireframe, textured);
    }

    void releaseGL() { renderer_.releaseGL(); loaded_ = false; }

    int textureCount() const { return renderer_.textureCount(); }
    int meshCount()    const { return renderer_.meshCount(); }

private:
    NsbmdTextureRenderer renderer_;
    bool loaded_ = false;

    // Minimal NSBMD section finder: scans for a 4-byte magic value.
    // Real NSBMD uses a section-offset table in the file header; adjust
    // this for your actual header parser if needed.
    static std::span<const uint8_t> findSection(std::span<const uint8_t> data,
                                                 uint32_t magic)
    {
        if (data.size() < 16) return {};
        // NSBMD header: bytes 0–3 = file magic, 4–5 = BOM, 6–7 = version,
        // 8–11 = file size, 12–13 = header size, 14–15 = section count.
        // Sections immediately follow the header.
        size_t pos = 16; // skip file header
        while (pos + 8 <= data.size())
        {
            uint32_t secMagic = static_cast<uint32_t>(data[pos+0])
                              | (static_cast<uint32_t>(data[pos+1]) << 8)
                              | (static_cast<uint32_t>(data[pos+2]) << 16)
                              | (static_cast<uint32_t>(data[pos+3]) << 24);
            uint32_t secSize  = static_cast<uint32_t>(data[pos+4])
                              | (static_cast<uint32_t>(data[pos+5]) << 8)
                              | (static_cast<uint32_t>(data[pos+6]) << 16)
                              | (static_cast<uint32_t>(data[pos+7]) << 24);
            if (secSize == 0 || secSize > data.size()) break;

            if (secMagic == magic && pos + secSize <= data.size())
                return data.subspan(pos, secSize);

            pos += secSize;
        }
        return {};
    }
};
