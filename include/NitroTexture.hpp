#pragma once
// =============================================================================
//  NitroTexture.hpp
//  Shared texture descriptor for the NDS Nitro GX pipeline.
//
//  This header is the single source of truth for the decoded texture type.
//  Both Tex0Parser and NsbmdTextureRenderer include this header; they must
//  never define their own conflicting struct.
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// DS texture formats (TEX0 format field, 3 bits)
// ---------------------------------------------------------------------------
enum class NitroTexFmt : uint8_t
{
    None        = 0,
    A3I5        = 1,   // 8-bit:  3-bit alpha + 5-bit palette index
    Palette4    = 2,   // 2 bpp palette
    Palette16   = 3,   // 4 bpp palette
    Palette256  = 4,   // 8 bpp palette
    Compressed  = 5,   // 4×4 texel block compression
    A5I3        = 6,   // 8-bit:  5-bit alpha + 3-bit palette index
    Direct      = 7,   // 16-bit direct ABGR1555
};

// ---------------------------------------------------------------------------
// NitroTexture
//  Holds one fully decoded texture ready for OpenGL upload.
//
//  Key fields
//  ----------
//  texAddr  – GX address (texImageParam bits[15:0] << 3).
//             The renderer uses this as the key in texAddrToGL.
//  rgba8    – RGBA8 pixel data, width×height×4 bytes, row-major, Y-up.
//             Populated by Tex0Parser::decode().
// ---------------------------------------------------------------------------
struct NitroTexture
{
    // Identification
    std::string  name;          ///< Name from TEX0 section (null-terminated, max 16 chars)

    // GX hardware address — primary lookup key
    uint32_t     texAddr   = 0; ///< (texImageParam & 0xFFFF) << 3

    // Raw TEX0 fields
    uint32_t     texImageParam = 0; ///< Full texImageParam word from TEX0 header

    // Dimensions (power-of-two, 8–1024)
    uint16_t     width     = 0;
    uint16_t     height    = 0;

    // Format
    NitroTexFmt  format    = NitroTexFmt::None;
    bool         hasAlpha  = false; ///< True for A3I5, A5I3, Direct, Compressed

    // Palette info (only valid for indexed formats)
    uint16_t     paletteOffset = 0; ///< Byte offset inside PLTT block
    uint16_t     paletteSize   = 0; ///< Number of palette colours

    // Decoded pixel data (empty until decode() is called)
    std::vector<uint8_t> rgba8; ///< width × height × 4 bytes (RGBA8)

    // OpenGL handle (0 = not uploaded yet)
    uint32_t     glTexID   = 0;

    // Validity
    bool isValid() const { return width > 0 && height > 0 && format != NitroTexFmt::None; }
};

// Convenience: width/height encoded in texImageParam bits[22:20] and [25:23]
//   bits[22:20] → sizeS index  (8 << index)
//   bits[25:23] → sizeT index  (8 << index)
inline uint16_t nitroTexWidth(uint32_t texImageParam)
{
    return static_cast<uint16_t>(8u << ((texImageParam >> 20) & 0x7u));
}
inline uint16_t nitroTexHeight(uint32_t texImageParam)
{
    return static_cast<uint16_t>(8u << ((texImageParam >> 23) & 0x7u));
}
inline NitroTexFmt nitroTexFormat(uint32_t texImageParam)
{
    return static_cast<NitroTexFmt>((texImageParam >> 26) & 0x7u);
}
inline uint32_t nitroTexAddr(uint32_t texImageParam)
{
    // Hardware address = lower 16 bits × 8
    return (texImageParam & 0x0000FFFFu) << 3u;
}
