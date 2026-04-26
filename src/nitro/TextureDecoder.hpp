#pragma once

// ============================================================================
// TextureDecoder.hpp  —  Nintendo DS Texture Format Decoder  (v0.3.0)
// ============================================================================
//
// Decodes raw NDS pixel data (from ParsedTex0) into RGBA8 images suitable
// for direct upload via glTexImage2D.
//
// Supported formats:
//   A3I5   — 8 bpp: 3-bit alpha, 5-bit palette index
//   A5I3   — 8 bpp: 5-bit alpha, 3-bit palette index
//   PAL4   — 2 bpp: 4-color palette
//   PAL16  — 4 bpp: 16-color palette
//   PAL256 — 8 bpp: 256-color palette
//   Direct — 16 bpp: BGR555 no palette (bit 15 = opaque flag)
//
// Palette color format: BGR555 (little-endian 16-bit):
//   bits [4:0]  R
//   bits [9:5]  G
//   bits [14:10] B
//   bit  [15]    (in some formats) alpha/special
//
// Color 0 in paletted textures: transparent when NitroRawTexture::color0Transparent.
//
// V-flip: DS origin is top-left (V increases down); OpenGL origin is
// bottom-left (V increases up). We flip rows during decode so the output
// RGBA data can be uploaded with GL_TEXTURE_2D without any shader transform.
// ============================================================================

#include <vector>

#include "nitro/NitroTexture.hpp"

namespace nitro
{
    class TextureDecoder
    {
    public:
        // Decode a single raw texture to RGBA8.
        // tex.dataOffset and tex.paletteDataOffset are relative to the blobs
        // held in parsedTex0.
        static DecodedTexture decode(const NitroRawTexture& tex,
                                     const ParsedTex0&       parsedTex0);

        // Convenience: decode all textures in a ParsedTex0.
        static std::vector<DecodedTexture> decodeAll(const ParsedTex0& parsedTex0);
    };

} // namespace nitro
