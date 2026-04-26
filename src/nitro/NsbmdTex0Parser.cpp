#include "NsbmdTex0Parser.h"
#include <cstring>

static uint16_t readU16(const uint8_t* p) { return *(uint16_t*)p; }
static uint32_t readU32(const uint8_t* p) { return *(uint32_t*)p; }

Tex0Data NsbmdTex0Parser::Parse(const uint8_t* data, size_t size)
{
    Tex0Data result;

    // Locate TEX0 header
    const uint8_t* ptr = data;

    uint16_t texCount = readU16(ptr + 0x0A);
    uint16_t palCount = readU16(ptr + 0x0C);

    uint32_t texInfoOffset = readU32(ptr + 0x10);
    uint32_t palInfoOffset = readU32(ptr + 0x14);

    // --- TEXTURES ---
    for (int i = 0; i < texCount; i++)
    {
        const uint8_t* entry = data + texInfoOffset + i * 0x20;

        DsTexture tex;

        tex.width  = 8 << (entry[0] & 0x7);
        tex.height = 8 << ((entry[0] >> 3) & 0x7);

        tex.format = (entry[1] >> 5) & 0x7;

        tex.dataOffset = readU32(entry + 0x04);
        tex.paletteIndex = readU16(entry + 0x08);

        result.textures.push_back(tex);
    }

    // --- PALETTES ---
    for (int i = 0; i < palCount; i++)
    {
        const uint8_t* entry = data + palInfoOffset + i * 0x10;

        DsPalette pal;

        uint32_t palDataOffset = readU32(entry + 0x04);
        uint32_t palSize = readU32(entry + 0x08);

        int colorCount = palSize / 2;

        pal.colors.resize(colorCount);

        memcpy(pal.colors.data(), data + palDataOffset, palSize);

        result.palettes.push_back(pal);
    }

    return result;
}