#pragma once
#include <vector>
#include <cstdint>
#include "NsbmdTex0Parser.h"

struct DecodedTexture
{
    int width;
    int height;
    std::vector<uint8_t> rgba;
};

class DsTextureDecoder
{
public:
    static DecodedTexture Decode(
        const DsTexture& tex,
        const uint8_t* texData,
        const DsPalette& palette
    );
};