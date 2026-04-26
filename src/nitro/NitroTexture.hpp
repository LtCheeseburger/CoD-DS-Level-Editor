#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nitro
{
    enum class NitroTexFmt
    {
        A3I5,
        A5I3,
        Color4,
        Color16,
        Color256,
        Texel4x4,
        Direct
    };

    struct DecodedTexture
    {
        std::string name;

        uint32_t width  = 0;
        uint32_t height = 0;

        NitroTexFmt format;

        std::vector<uint8_t> pixels; // RGBA8

        // 🔥 NEW: DS texture address (from TEXIMAGE_PARAM)
        uint32_t texAddr = 0;
    };
}