#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nitro
{
    enum class NitroTexFmt : std::uint8_t
    {
        None       = 0,
        A3I5       = 1,
        Palette4   = 2,
        Palette16  = 3,
        Palette256 = 4,
        Tex4x4     = 5,
        A5I3       = 6,
        Direct     = 7,
    };

    struct NitroRawTexture
    {
        std::string name;

        std::uint16_t widthPx  = 0;
        std::uint16_t heightPx = 0;

        NitroTexFmt format = NitroTexFmt::None;

        std::uint32_t texImageParam = 0;
        std::uint32_t texAddr       = 0; // (texImageParam & 0xFFFF) << 3

        std::uint32_t dataOffset        = 0; // byte offset in ParsedTex0::textureData
        std::uint32_t dataSize          = 0; // byte size in textureData
        std::uint32_t paletteDataOffset = 0; // byte offset in ParsedTex0::paletteData
        std::uint32_t paletteColorCount = 0;

        bool color0Transparent = false;
        bool hasAlpha          = false;

        bool valid = false;
    };

    struct DecodedTexture
    {
        std::string name;

        std::uint32_t width  = 0;
        std::uint32_t height = 0;

        NitroTexFmt format = NitroTexFmt::None;

        std::vector<std::uint8_t> rgba;

        std::uint32_t texAddr = 0;

        bool valid = false;
    };

    struct ParsedTex0
    {
        std::vector<NitroRawTexture> textures;

        std::vector<std::uint8_t> textureData;
        std::vector<std::uint8_t> paletteData;

        bool valid = false;
    };

} // namespace nitro
