#include "TextureDecoder.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <sstream>

// ============================================================================
// TextureDecoder.cpp  —  Nintendo DS Texture Format Decoder  (v0.3.0)
// ============================================================================

namespace nitro
{
    namespace
    {
        // ------------------------------------------------------------------ //
        //  BGR555 → RGBA8                                                     //
        // ------------------------------------------------------------------ //
        //
        // DS palette entry:  16-bit little-endian
        //   bits [4:0]   R  (5 bits)
        //   bits [9:5]   G  (5 bits)
        //   bits [14:10] B  (5 bits)
        //   bit  [15]    reserved / alpha (only used by Direct format)
        //
        // We expand 5-bit channels to 8-bit by: val8 = (val5 * 255 + 15) / 31
        // (This is the standard bit-replication formula that gives 0→0, 31→255.)

        struct RGBA { std::uint8_t r, g, b, a; };

        inline RGBA bgr555ToRgba(std::uint16_t bgr555, std::uint8_t alpha = 255)
        {
            const std::uint8_t r5 =  bgr555        & 0x1Fu;
            const std::uint8_t g5 = (bgr555 >>  5) & 0x1Fu;
            const std::uint8_t b5 = (bgr555 >> 10) & 0x1Fu;

            return {
                static_cast<std::uint8_t>((r5 * 255u + 15u) / 31u),
                static_cast<std::uint8_t>((g5 * 255u + 15u) / 31u),
                static_cast<std::uint8_t>((b5 * 255u + 15u) / 31u),
                alpha
            };
        }

        // ------------------------------------------------------------------ //
        //  Palette loading helper                                              //
        // ------------------------------------------------------------------ //
        //
        // Reads `count` BGR555 entries from paletteData starting at byteOffset.
        // Returns RGBA8 colors. Clamps count if out of bounds.

        std::vector<RGBA> loadPalette(const std::vector<std::uint8_t>& paletteData,
                                      std::size_t                       byteOffset,
                                      std::size_t                       count,
                                      bool                              color0Transparent)
        {
            std::vector<RGBA> pal(count, RGBA{0, 0, 0, 0});

            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t off = byteOffset + i * 2u;
                if (off + 2u > paletteData.size()) break;

                const std::uint16_t bgr =
                    static_cast<std::uint16_t>(paletteData[off]) |
                    (static_cast<std::uint16_t>(paletteData[off + 1]) << 8);

                if (i == 0 && color0Transparent)
                    pal[i] = RGBA{0, 0, 0, 0};
                else
                    pal[i] = bgr555ToRgba(bgr);
            }

            return pal;
        }

        // ------------------------------------------------------------------ //
        //  V-flip helper                                                       //
        // ------------------------------------------------------------------ //
        //
        // DS stores rows top-to-bottom; OpenGL expects bottom-to-top.
        // We flip in-place so the caller can do a straight glTexImage2D upload.

        void vFlipRgba(std::vector<std::uint8_t>& rgba, int w, int h)
        {
            const std::size_t rowBytes = static_cast<std::size_t>(w) * 4u;
            for (int top = 0, bot = h - 1; top < bot; ++top, --bot)
            {
                std::uint8_t* rowTop = rgba.data() + static_cast<std::size_t>(top) * rowBytes;
                std::uint8_t* rowBot = rgba.data() + static_cast<std::size_t>(bot) * rowBytes;
                std::swap_ranges(rowTop, rowTop + rowBytes, rowBot);
            }
        }

        // ------------------------------------------------------------------ //
        //  Decode A3I5  (8 bpp: 3-bit alpha, 5-bit index)                    //
        // ------------------------------------------------------------------ //

        std::vector<std::uint8_t> decodeA3I5(const NitroRawTexture&           tex,
                                              const std::vector<std::uint8_t>& texData,
                                              const std::vector<std::uint8_t>& palData)
        {
            const int w = tex.widthPx, h = tex.heightPx;
            const auto pal = loadPalette(palData, tex.paletteDataOffset, 32u, false);

            std::vector<std::uint8_t> out(static_cast<std::size_t>(w * h * 4), 0);

            for (int i = 0; i < w * h; ++i)
            {
                const std::size_t srcOff = tex.dataOffset + static_cast<std::size_t>(i);
                if (srcOff >= texData.size()) break;

                const std::uint8_t byte  = texData[srcOff];
                const std::uint8_t idx   = byte & 0x1Fu;          // bits [4:0]
                const std::uint8_t alpha3= (byte >> 5) & 0x07u;  // bits [7:5]

                // Expand 3-bit alpha to 8-bit: (a3 * 255 + 3) / 7
                const std::uint8_t alpha8 = static_cast<std::uint8_t>((alpha3 * 255u + 3u) / 7u);

                const RGBA c = (idx < pal.size()) ? pal[idx] : RGBA{0,0,0,0};

                const std::size_t dst = static_cast<std::size_t>(i) * 4u;
                out[dst + 0] = c.r;
                out[dst + 1] = c.g;
                out[dst + 2] = c.b;
                out[dst + 3] = alpha8;
            }

            vFlipRgba(out, w, h);
            return out;
        }

        // ------------------------------------------------------------------ //
        //  Decode A5I3  (8 bpp: 5-bit alpha, 3-bit index)                    //
        // ------------------------------------------------------------------ //

        std::vector<std::uint8_t> decodeA5I3(const NitroRawTexture&           tex,
                                              const std::vector<std::uint8_t>& texData,
                                              const std::vector<std::uint8_t>& palData)
        {
            const int w = tex.widthPx, h = tex.heightPx;
            const auto pal = loadPalette(palData, tex.paletteDataOffset, 8u, false);

            std::vector<std::uint8_t> out(static_cast<std::size_t>(w * h * 4), 0);

            for (int i = 0; i < w * h; ++i)
            {
                const std::size_t srcOff = tex.dataOffset + static_cast<std::size_t>(i);
                if (srcOff >= texData.size()) break;

                const std::uint8_t byte  = texData[srcOff];
                const std::uint8_t idx   = byte & 0x07u;           // bits [2:0]
                const std::uint8_t alpha5= (byte >> 3) & 0x1Fu;   // bits [7:3]

                // Expand 5-bit alpha to 8-bit
                const std::uint8_t alpha8 = static_cast<std::uint8_t>((alpha5 * 255u + 15u) / 31u);

                const RGBA c = (idx < pal.size()) ? pal[idx] : RGBA{0,0,0,0};

                const std::size_t dst = static_cast<std::size_t>(i) * 4u;
                out[dst + 0] = c.r;
                out[dst + 1] = c.g;
                out[dst + 2] = c.b;
                out[dst + 3] = alpha8;
            }

            vFlipRgba(out, w, h);
            return out;
        }

        // ------------------------------------------------------------------ //
        //  Decode PAL4  (2 bpp: 4-color palette)                             //
        // ------------------------------------------------------------------ //

        std::vector<std::uint8_t> decodePal4(const NitroRawTexture&           tex,
                                              const std::vector<std::uint8_t>& texData,
                                              const std::vector<std::uint8_t>& palData)
        {
            const int w = tex.widthPx, h = tex.heightPx;
            const auto pal = loadPalette(palData, tex.paletteDataOffset, 4u, tex.color0Transparent);

            std::vector<std::uint8_t> out(static_cast<std::size_t>(w * h * 4), 0);

            // 4 pixels per byte (2 bpp), packed LSB-first
            for (int i = 0; i < w * h; ++i)
            {
                const std::size_t byteIdx = tex.dataOffset + static_cast<std::size_t>(i) / 4u;
                if (byteIdx >= texData.size()) break;

                const int         shift = (i % 4) * 2;
                const std::uint8_t idx  = (texData[byteIdx] >> shift) & 0x03u;

                const RGBA c = (idx < pal.size()) ? pal[idx] : RGBA{0,0,0,0};

                const std::size_t dst = static_cast<std::size_t>(i) * 4u;
                out[dst + 0] = c.r;
                out[dst + 1] = c.g;
                out[dst + 2] = c.b;
                out[dst + 3] = c.a;
            }

            vFlipRgba(out, w, h);
            return out;
        }

        // ------------------------------------------------------------------ //
        //  Decode PAL16  (4 bpp: 16-color palette)                           //
        // ------------------------------------------------------------------ //

        std::vector<std::uint8_t> decodePal16(const NitroRawTexture&           tex,
                                               const std::vector<std::uint8_t>& texData,
                                               const std::vector<std::uint8_t>& palData)
        {
            const int w = tex.widthPx, h = tex.heightPx;
            const auto pal = loadPalette(palData, tex.paletteDataOffset, 16u, tex.color0Transparent);

            std::vector<std::uint8_t> out(static_cast<std::size_t>(w * h * 4), 0);

            // 2 pixels per byte (4 bpp), packed LSB-first
            for (int i = 0; i < w * h; ++i)
            {
                const std::size_t byteIdx = tex.dataOffset + static_cast<std::size_t>(i) / 2u;
                if (byteIdx >= texData.size()) break;

                const int         shift = (i % 2) * 4;
                const std::uint8_t idx  = (texData[byteIdx] >> shift) & 0x0Fu;

                const RGBA c = (idx < pal.size()) ? pal[idx] : RGBA{0,0,0,0};

                const std::size_t dst = static_cast<std::size_t>(i) * 4u;
                out[dst + 0] = c.r;
                out[dst + 1] = c.g;
                out[dst + 2] = c.b;
                out[dst + 3] = c.a;
            }

            vFlipRgba(out, w, h);
            return out;
        }

        // ------------------------------------------------------------------ //
        //  Decode PAL256  (8 bpp: 256-color palette)                         //
        // ------------------------------------------------------------------ //

        std::vector<std::uint8_t> decodePal256(const NitroRawTexture&           tex,
                                                const std::vector<std::uint8_t>& texData,
                                                const std::vector<std::uint8_t>& palData)
        {
            const int w = tex.widthPx, h = tex.heightPx;
            const auto pal = loadPalette(palData, tex.paletteDataOffset, 256u, tex.color0Transparent);

            std::vector<std::uint8_t> out(static_cast<std::size_t>(w * h * 4), 0);

            for (int i = 0; i < w * h; ++i)
            {
                const std::size_t srcOff = tex.dataOffset + static_cast<std::size_t>(i);
                if (srcOff >= texData.size()) break;

                const std::uint8_t idx = texData[srcOff];
                const RGBA c = (idx < pal.size()) ? pal[idx] : RGBA{0,0,0,0};

                const std::size_t dst = static_cast<std::size_t>(i) * 4u;
                out[dst + 0] = c.r;
                out[dst + 1] = c.g;
                out[dst + 2] = c.b;
                out[dst + 3] = c.a;
            }

            vFlipRgba(out, w, h);
            return out;
        }

        // ------------------------------------------------------------------ //
        //  Decode Direct  (16 bpp: BGR555, bit 15 = alpha)                   //
        // ------------------------------------------------------------------ //

        std::vector<std::uint8_t> decodeDirect(const NitroRawTexture&           tex,
                                                const std::vector<std::uint8_t>& texData)
        {
            const int w = tex.widthPx, h = tex.heightPx;
            std::vector<std::uint8_t> out(static_cast<std::size_t>(w * h * 4), 0);

            for (int i = 0; i < w * h; ++i)
            {
                const std::size_t srcOff = tex.dataOffset + static_cast<std::size_t>(i) * 2u;
                if (srcOff + 2u > texData.size()) break;

                const std::uint16_t raw =
                    static_cast<std::uint16_t>(texData[srcOff]) |
                    (static_cast<std::uint16_t>(texData[srcOff + 1]) << 8);

                // Bit 15: 0 = transparent, 1 = opaque
                const std::uint8_t alpha = (raw & 0x8000u) ? 255u : 0u;
                const RGBA c = bgr555ToRgba(raw & 0x7FFFu, alpha);

                const std::size_t dst = static_cast<std::size_t>(i) * 4u;
                out[dst + 0] = c.r;
                out[dst + 1] = c.g;
                out[dst + 2] = c.b;
                out[dst + 3] = c.a;
            }

            vFlipRgba(out, w, h);
            return out;
        }

    } // anonymous namespace

    // ======================================================================= //
    //  Public API                                                              //
    // ======================================================================= //

    DecodedTexture TextureDecoder::decode(const NitroRawTexture& tex,
                                          const ParsedTex0&       parsedTex0)
    {
        DecodedTexture out;
        out.name   = tex.name;
        out.width  = tex.widthPx;
        out.height = tex.heightPx;

        if (tex.widthPx == 0 || tex.heightPx == 0)
        {
            core::Logger::warning("TextureDecoder: zero-size texture \"" + tex.name + "\"");
            return out;
        }

        const auto& td = parsedTex0.textureData;
        const auto& pd = parsedTex0.paletteData;

        // Bounds-check the data region declared in the header.
        if (tex.dataOffset + tex.dataSize > td.size())
        {
            // Try anyway if at least some data exists
            if (tex.dataOffset >= td.size())
            {
                core::Logger::warning("TextureDecoder: texture data out of bounds \"" + tex.name + "\"");
                return out;
            }
        }

        switch (tex.format)
        {
            case NitroTexFmt::A3I5:
                out.rgba = decodeA3I5(tex, td, pd);
                break;

            case NitroTexFmt::A5I3:
                out.rgba = decodeA5I3(tex, td, pd);
                break;

            case NitroTexFmt::Palette4:
                out.rgba = decodePal4(tex, td, pd);
                break;

            case NitroTexFmt::Palette16:
                out.rgba = decodePal16(tex, td, pd);
                break;

            case NitroTexFmt::Palette256:
                out.rgba = decodePal256(tex, td, pd);
                break;

            case NitroTexFmt::Direct:
                out.rgba = decodeDirect(tex, td);
                break;

            case NitroTexFmt::Tex4x4:
                core::Logger::warning("TextureDecoder: Tex4x4 not supported (\"" + tex.name + "\")");
                return out;

            default:
                core::Logger::warning("TextureDecoder: unknown format for \"" + tex.name + "\"");
                return out;
        }

        const std::size_t expectedBytes =
            static_cast<std::size_t>(tex.widthPx) * tex.heightPx * 4u;

        if (out.rgba.size() != expectedBytes)
        {
            core::Logger::warning("TextureDecoder: decoded size mismatch for \"" + tex.name + "\"");
            out.rgba.resize(expectedBytes, 0);
        }

        out.valid = true;

        std::ostringstream ss;
        ss << "TextureDecoder: decoded \"" << tex.name << "\""
           << " " << tex.widthPx << "x" << tex.heightPx
           << " fmt=" << static_cast<int>(tex.format);
        core::Logger::info(ss.str());

        return out;
    }

    std::vector<DecodedTexture> TextureDecoder::decodeAll(const ParsedTex0& parsedTex0)
    {
        std::vector<DecodedTexture> result;
        result.reserve(parsedTex0.textures.size());

        for (const NitroRawTexture& raw : parsedTex0.textures)
            result.push_back(decode(raw, parsedTex0));

        return result;
    }

} // namespace nitro
