// =============================================================================
//  Tex0Parser.cpp
//  Implementation of Tex0Parser: parse + decode all DS texture formats.
//
//  TEX0 binary layout (all values little-endian)
//  -----------------------------------------------
//  Offset  Size  Meaning
//  0x00    2     texture data size / 8
//  0x02    2     info offset (relative to block start)
//  0x04    4     reserved / padding
//  0x08    4     texture data offset (from block start)
//  0x0C    4     reserved
//  0x10    2     palette data size / 8
//  0x12    2     reserved
//  0x14    4     palette data offset (from block start)
//  0x18+       Dictionary (see below)
//
//  Dictionary format:
//  0x00  2  dummy / revision (usually 0)
//  0x02  2  N = number of entries
//  0x04  N×(4+16+4) = N × NameEntry
//       each NameEntry: u32 dataOffset(unused here), char[16] name, u32 texImageParam
//  (in practice the NDS dict has: u16 count, followed by name/param pairs)
//
//  Actual NDS NSBMD TEX0 dictionary layout used here:
//  at infoOffset+0:  u16 texCount
//  at infoOffset+2:  u16 plttCount
//  then texCount × {char[16] name, u32 texImageParam, u16 paletteOffset, u16 paletteSize}
//  then plttCount × {char[16] name, u32 plttOffset}
//
//  Reference: Nitro SDK / GBAtek NDS 3D chapter.
// =============================================================================

#include "Tex0Parser.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{

// Safe little-endian reads from a byte buffer
inline uint8_t  read8 (const uint8_t* p) { return *p; }
inline uint16_t read16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline uint32_t read32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

// Clamp to buffer range
template<typename T>
bool inRange(const std::span<const uint8_t>& buf, size_t offset, size_t count = 1)
{
    return (offset + sizeof(T) * count) <= buf.size();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// bgr555ToRgba8
// ---------------------------------------------------------------------------
/*static*/ uint32_t Tex0Parser::bgr555ToRgba8(uint16_t bgr555, bool opaque)
{
    // DS colour: xBBBBBGGGGGRRRRR
    uint8_t r5 = (bgr555 >>  0) & 0x1F;
    uint8_t g5 = (bgr555 >>  5) & 0x1F;
    uint8_t b5 = (bgr555 >> 10) & 0x1F;
    // Scale 5-bit to 8-bit: v8 = (v5 << 3) | (v5 >> 2)
    uint8_t r = (r5 << 3) | (r5 >> 2);
    uint8_t g = (g5 << 3) | (g5 >> 2);
    uint8_t b = (b5 << 3) | (b5 >> 2);
    uint8_t a = opaque ? 255u : 0u;
    return (static_cast<uint32_t>(r))
         | (static_cast<uint32_t>(g) << 8)
         | (static_cast<uint32_t>(b) << 16)
         | (static_cast<uint32_t>(a) << 24);
}

// ---------------------------------------------------------------------------
// parse()
// ---------------------------------------------------------------------------
ParsedTex0 Tex0Parser::parse(std::span<const uint8_t> tex0Data) const
{
    ParsedTex0 result;
    result.valid = false;

    if (tex0Data.size() < 0x20)
        return result;

    const uint8_t* base = tex0Data.data();

    // Header
    // uint16_t texDataSize8  = read16(base + 0x00); // × 8 = byte size
    uint16_t infoOffset    = read16(base + 0x02);
    uint32_t texDataOffset = read32(base + 0x08);
    // uint16_t plttDataSize8 = read16(base + 0x10);
    uint32_t plttDataOffset = read32(base + 0x14);

    if (infoOffset == 0 || infoOffset >= tex0Data.size())
        return result;

    // ---------- Dictionary ----------
    // NDS NSBMD uses a specific dict layout.  We read:
    //   at [infoOffset+0x00]: u8  revision (skip)
    //   at [infoOffset+0x01]: u8  texCount
    //   at [infoOffset+0x02]: u16 paletteCount (skip for now)
    //   then texCount entries of:
    //       u16 texDataSizeShifted (×8 = size)
    //       u16 paletteOffset
    //       u32 texImageParam
    //       u16 extPaletteAttr (skip)
    //       u16 reserved
    //   then texCount × 16-byte names
    //
    // (Different NSBMD versions pack things differently.  We use the layout
    //  seen in CoD DS NSBMD files, which matches the Nitro SDK 2.x format.)

    const uint8_t* info = base + infoOffset;
    size_t infoAvail    = tex0Data.size() - infoOffset;

    if (infoAvail < 4)
        return result;

    // Skip u16 dummy, then u16 count
    uint16_t texCount = read16(info + 0x02);

    if (texCount == 0 || texCount > 512)
        return result;

    // Each entry: 8 bytes params + 16 bytes name = 24 bytes
    constexpr size_t kEntrySize = 8;   // param block
    constexpr size_t kNameSize  = 16;
    size_t paramBlock  = static_cast<size_t>(texCount) * kEntrySize;
    size_t nameBlock   = static_cast<size_t>(texCount) * kNameSize;

    if (infoAvail < 4 + paramBlock + nameBlock)
        return result;

    const uint8_t* params = info + 0x04;
    const uint8_t* names  = info + 0x04 + paramBlock;

    result.textures.reserve(texCount);

    for (uint16_t i = 0; i < texCount; ++i)
    {
        NitroTexture tex;

        // Name (up to 16 chars, null-terminated)
        char nameBuf[17] = {};
        std::memcpy(nameBuf, names + i * kNameSize, 16);
        tex.name = std::string(nameBuf);

        // Param block for entry i
        const uint8_t* ep = params + i * kEntrySize;

        uint16_t texSizeShifted = read16(ep + 0x00); // texData size >> 3
        uint16_t paletteOffset  = read16(ep + 0x02); // palette offset >> 3
        uint32_t texImageParam  = read32(ep + 0x04);

        tex.texImageParam  = texImageParam;
        tex.texAddr        = nitroTexAddr(texImageParam);
        tex.width          = nitroTexWidth(texImageParam);
        tex.height         = nitroTexHeight(texImageParam);
        tex.format         = nitroTexFormat(texImageParam);
        tex.paletteOffset  = paletteOffset;
        tex.paletteSize    = texSizeShifted; // approximate; refined at decode time

        // Validate
        if (!tex.isValid())
        {
            result.textures.push_back(std::move(tex));
            continue;
        }

        tex.hasAlpha = (tex.format == NitroTexFmt::A3I5
                     || tex.format == NitroTexFmt::A5I3
                     || tex.format == NitroTexFmt::Direct
                     || tex.format == NitroTexFmt::Compressed);

        result.textures.push_back(std::move(tex));
    }

    result.valid = true;
    return result;
}

// ---------------------------------------------------------------------------
// decode()  – fill rgba8 for all valid textures
// ---------------------------------------------------------------------------
void Tex0Parser::decode(ParsedTex0& parsed, std::span<const uint8_t> tex0Data) const
{
    if (!parsed.valid) return;

    const uint8_t* base = tex0Data.data();
    uint32_t texDataOffset  = read32(base + 0x08);
    uint32_t plttDataOffset = read32(base + 0x14);

    // Build safe sub-spans
    std::span<const uint8_t> texData;
    if (texDataOffset < tex0Data.size())
        texData = tex0Data.subspan(texDataOffset);

    std::span<const uint8_t> plttData;
    if (plttDataOffset < tex0Data.size())
        plttData = tex0Data.subspan(plttDataOffset);

    for (NitroTexture& tex : parsed.textures)
    {
        if (!tex.isValid()) continue;
        if (!texData.empty())
        {
            // Slice texData starting at texAddr (relative to texData start)
            uint32_t offset = tex.texAddr;
            if (offset < texData.size())
            {
                std::span<const uint8_t> slicedTex = texData.subspan(offset);
                std::span<const uint8_t> slicedPltt;
                if (!plttData.empty() && tex.paletteOffset * 8 < plttData.size())
                    slicedPltt = plttData.subspan(tex.paletteOffset * 8u);

                switch (tex.format)
                {
                    case NitroTexFmt::A3I5:      decodeA3I5      (tex, slicedTex, slicedPltt); break;
                    case NitroTexFmt::Palette4:  decodePalette4  (tex, slicedTex, slicedPltt); break;
                    case NitroTexFmt::Palette16: decodePalette16 (tex, slicedTex, slicedPltt); break;
                    case NitroTexFmt::Palette256:decodePalette256(tex, slicedTex, slicedPltt); break;
                    case NitroTexFmt::Compressed:decodeCompressed(tex, slicedTex, slicedPltt); break;
                    case NitroTexFmt::A5I3:      decodeA5I3      (tex, slicedTex, slicedPltt); break;
                    case NitroTexFmt::Direct:    decodeDirect    (tex, slicedTex);             break;
                    default: break;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Format decoders
// ---------------------------------------------------------------------------

// Helper: read palette colour
static uint32_t readPaletteEntry(std::span<const uint8_t> pltt, size_t idx, bool transparent0 = false)
{
    size_t byteOff = idx * 2;
    if (byteOff + 1 >= pltt.size()) return 0;
    uint16_t bgr = static_cast<uint16_t>(pltt[byteOff]) | (static_cast<uint16_t>(pltt[byteOff+1]) << 8);
    bool opaque = !(transparent0 && idx == 0);
    return Tex0Parser::bgr555ToRgba8(bgr, opaque);
}

// ---- A3I5 (alpha3 + index5, 8bpp) ----------------------------------------
void Tex0Parser::decodeA3I5(NitroTexture& tex, std::span<const uint8_t> texData,
                             std::span<const uint8_t> plttData) const
{
    size_t pixels = static_cast<size_t>(tex.width) * tex.height;
    if (texData.size() < pixels) return;
    tex.rgba8.resize(pixels * 4);

    for (size_t i = 0; i < pixels; ++i)
    {
        uint8_t raw   = texData[i];
        uint8_t idx   = raw & 0x1F;
        uint8_t a3    = (raw >> 5) & 0x07;
        uint8_t alpha = (a3 << 5) | (a3 << 2) | (a3 >> 1); // expand to 8-bit

        uint32_t c = readPaletteEntry(plttData, idx, false);
        tex.rgba8[i*4+0] = (c >>  0) & 0xFF;
        tex.rgba8[i*4+1] = (c >>  8) & 0xFF;
        tex.rgba8[i*4+2] = (c >> 16) & 0xFF;
        tex.rgba8[i*4+3] = alpha;
    }
}

// ---- Palette4 (2bpp, 4 colours) -------------------------------------------
void Tex0Parser::decodePalette4(NitroTexture& tex, std::span<const uint8_t> texData,
                                 std::span<const uint8_t> plttData) const
{
    size_t pixels  = static_cast<size_t>(tex.width) * tex.height;
    size_t byteLen = (pixels + 3) / 4;
    if (texData.size() < byteLen) return;
    tex.rgba8.resize(pixels * 4);

    // Check texImageParam bit 29 for colour0 transparency
    bool transparent0 = (tex.texImageParam >> 29) & 1;

    for (size_t i = 0; i < pixels; ++i)
    {
        uint8_t byte = texData[i / 4];
        uint8_t idx  = (byte >> ((i % 4) * 2)) & 0x03;
        uint32_t c   = readPaletteEntry(plttData, idx, transparent0);
        tex.rgba8[i*4+0] = (c >>  0) & 0xFF;
        tex.rgba8[i*4+1] = (c >>  8) & 0xFF;
        tex.rgba8[i*4+2] = (c >> 16) & 0xFF;
        tex.rgba8[i*4+3] = (c >> 24) & 0xFF;
    }
}

// ---- Palette16 (4bpp, 16 colours) -----------------------------------------
void Tex0Parser::decodePalette16(NitroTexture& tex, std::span<const uint8_t> texData,
                                  std::span<const uint8_t> plttData) const
{
    size_t pixels  = static_cast<size_t>(tex.width) * tex.height;
    size_t byteLen = (pixels + 1) / 2;
    if (texData.size() < byteLen) return;
    tex.rgba8.resize(pixels * 4);

    bool transparent0 = (tex.texImageParam >> 29) & 1;

    for (size_t i = 0; i < pixels; ++i)
    {
        uint8_t byte = texData[i / 2];
        uint8_t idx  = (i % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
        uint32_t c   = readPaletteEntry(plttData, idx, transparent0);
        tex.rgba8[i*4+0] = (c >>  0) & 0xFF;
        tex.rgba8[i*4+1] = (c >>  8) & 0xFF;
        tex.rgba8[i*4+2] = (c >> 16) & 0xFF;
        tex.rgba8[i*4+3] = (c >> 24) & 0xFF;
    }
}

// ---- Palette256 (8bpp, 256 colours) ----------------------------------------
void Tex0Parser::decodePalette256(NitroTexture& tex, std::span<const uint8_t> texData,
                                   std::span<const uint8_t> plttData) const
{
    size_t pixels = static_cast<size_t>(tex.width) * tex.height;
    if (texData.size() < pixels) return;
    tex.rgba8.resize(pixels * 4);

    bool transparent0 = (tex.texImageParam >> 29) & 1;

    for (size_t i = 0; i < pixels; ++i)
    {
        uint8_t  idx = texData[i];
        uint32_t c   = readPaletteEntry(plttData, idx, transparent0);
        tex.rgba8[i*4+0] = (c >>  0) & 0xFF;
        tex.rgba8[i*4+1] = (c >>  8) & 0xFF;
        tex.rgba8[i*4+2] = (c >> 16) & 0xFF;
        tex.rgba8[i*4+3] = (c >> 24) & 0xFF;
    }
}

// ---- Compressed (4×4 texel blocks) -----------------------------------------
// Each 4×4 block = 32 bits texel data + 16 bits palette slot info
// Palette slot info lives in the second half of VRAM (offset by texData_size/2)
void Tex0Parser::decodeCompressed(NitroTexture& tex, std::span<const uint8_t> texData,
                                   std::span<const uint8_t> plttData) const
{
    if (tex.width < 4 || tex.height < 4) return;

    size_t pixels     = static_cast<size_t>(tex.width) * tex.height;
    size_t blocksX    = tex.width  / 4;
    size_t blocksY    = tex.height / 4;
    size_t totalBlocks = blocksX * blocksY;

    // Each block: 4 bytes texels + 2 bytes slot info
    // Slot info offset = texData.size() / 2 (hardware partitions VRAM)
    size_t slotOffset = texData.size() / 2;

    if (texData.size() < totalBlocks * 4 + totalBlocks * 2) return;

    tex.rgba8.resize(pixels * 4, 0);

    for (size_t by = 0; by < blocksY; ++by)
    {
        for (size_t bx = 0; bx < blocksX; ++bx)
        {
            size_t blockIdx = by * blocksX + bx;

            // 4 bytes of 2-bit indices for the 4×4 block
            const uint8_t* texelPtr = texData.data() + blockIdx * 4;

            // 2-byte slot info
            size_t slotIdx = slotOffset + blockIdx * 2;
            if (slotIdx + 1 >= texData.size()) continue;
            uint16_t slotInfo = static_cast<uint16_t>(texData[slotIdx])
                              | (static_cast<uint16_t>(texData[slotIdx+1]) << 8);

            uint16_t paletteBase = (slotInfo & 0x3FFF) * 4; // *4 colours
            uint8_t  mode        = (slotInfo >> 14) & 0x03;

            // Build 4-colour sub-palette
            uint32_t pal[4] = {};
            auto readC = [&](size_t ci) -> uint32_t
            {
                size_t off = (paletteBase + ci) * 2;
                if (off + 1 >= plttData.size()) return 0xFF000000u;
                uint16_t bgr = static_cast<uint16_t>(plttData[off])
                             | (static_cast<uint16_t>(plttData[off+1]) << 8);
                return bgr555ToRgba8(bgr, true);
            };

            switch (mode)
            {
                case 0: // c0, c1, c2=transparent, c3=transparent
                    pal[0] = readC(0);
                    pal[1] = readC(1);
                    pal[2] = 0;
                    pal[3] = 0;
                    break;
                case 1: // c0, c1, c2=(c0+c1)/2, c3=transparent
                    pal[0] = readC(0);
                    pal[1] = readC(1);
                    {
                        uint8_t r = ((pal[0]&0xFF) + (pal[1]&0xFF)) / 2;
                        uint8_t g = (((pal[0]>>8)&0xFF) + ((pal[1]>>8)&0xFF)) / 2;
                        uint8_t b = (((pal[0]>>16)&0xFF) + ((pal[1]>>16)&0xFF)) / 2;
                        pal[2] = r | (g<<8) | (b<<16) | 0xFF000000u;
                    }
                    pal[3] = 0;
                    break;
                case 2: // c0, c1, c2, c3 (all opaque)
                    pal[0] = readC(0);
                    pal[1] = readC(1);
                    pal[2] = readC(2);
                    pal[3] = readC(3);
                    break;
                case 3: // c0, c1, c2=(5c0+3c1)/8, c3=(3c0+5c1)/8
                    pal[0] = readC(0);
                    pal[1] = readC(1);
                    for (int ch = 0; ch < 3; ++ch)
                    {
                        uint8_t c0 = (pal[0] >> (ch*8)) & 0xFF;
                        uint8_t c1 = (pal[1] >> (ch*8)) & 0xFF;
                        reinterpret_cast<uint8_t*>(&pal[2])[ch] = (5*c0 + 3*c1) / 8;
                        reinterpret_cast<uint8_t*>(&pal[3])[ch] = (3*c0 + 5*c1) / 8;
                    }
                    reinterpret_cast<uint8_t*>(&pal[2])[3] = 255;
                    reinterpret_cast<uint8_t*>(&pal[3])[3] = 255;
                    break;
            }

            // Write pixels
            for (int py = 0; py < 4; ++py)
            {
                uint8_t row = texelPtr[py];
                for (int px = 0; px < 4; ++px)
                {
                    uint8_t idx = (row >> (px * 2)) & 0x03;
                    uint32_t c  = pal[idx];

                    size_t dstX = bx * 4 + px;
                    size_t dstY = by * 4 + py;
                    size_t dstI = (dstY * tex.width + dstX) * 4;

                    if (dstI + 3 < tex.rgba8.size())
                    {
                        tex.rgba8[dstI+0] = (c >>  0) & 0xFF;
                        tex.rgba8[dstI+1] = (c >>  8) & 0xFF;
                        tex.rgba8[dstI+2] = (c >> 16) & 0xFF;
                        tex.rgba8[dstI+3] = (c >> 24) & 0xFF;
                    }
                }
            }
        }
    }
}

// ---- A5I3 (alpha5 + index3, 8bpp) -----------------------------------------
void Tex0Parser::decodeA5I3(NitroTexture& tex, std::span<const uint8_t> texData,
                             std::span<const uint8_t> plttData) const
{
    size_t pixels = static_cast<size_t>(tex.width) * tex.height;
    if (texData.size() < pixels) return;
    tex.rgba8.resize(pixels * 4);

    for (size_t i = 0; i < pixels; ++i)
    {
        uint8_t raw   = texData[i];
        uint8_t idx   = raw & 0x07;
        uint8_t a5    = (raw >> 3) & 0x1F;
        uint8_t alpha = (a5 << 3) | (a5 >> 2);

        uint32_t c = readPaletteEntry(plttData, idx, false);
        tex.rgba8[i*4+0] = (c >>  0) & 0xFF;
        tex.rgba8[i*4+1] = (c >>  8) & 0xFF;
        tex.rgba8[i*4+2] = (c >> 16) & 0xFF;
        tex.rgba8[i*4+3] = alpha;
    }
}

// ---- Direct (16bpp ABGR1555) -----------------------------------------------
void Tex0Parser::decodeDirect(NitroTexture& tex, std::span<const uint8_t> texData) const
{
    size_t pixels  = static_cast<size_t>(tex.width) * tex.height;
    size_t byteLen = pixels * 2;
    if (texData.size() < byteLen) return;
    tex.rgba8.resize(pixels * 4);

    for (size_t i = 0; i < pixels; ++i)
    {
        uint16_t raw = static_cast<uint16_t>(texData[i*2])
                     | (static_cast<uint16_t>(texData[i*2+1]) << 8);
        // bit 15 = alpha (1=opaque in ABGR1555)
        bool opaque  = (raw & 0x8000) != 0;
        uint16_t bgr = raw & 0x7FFF;
        uint32_t c   = bgr555ToRgba8(bgr, opaque);
        tex.rgba8[i*4+0] = (c >>  0) & 0xFF;
        tex.rgba8[i*4+1] = (c >>  8) & 0xFF;
        tex.rgba8[i*4+2] = (c >> 16) & 0xFF;
        tex.rgba8[i*4+3] = opaque ? 255u : 0u;
    }
}
