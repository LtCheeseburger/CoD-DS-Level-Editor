#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct DsTexture
{
    std::string name;

    uint16_t width;
    uint16_t height;

    uint32_t format;
    uint32_t dataOffset;

    uint32_t paletteIndex;
};

struct DsPalette
{
    std::string name;
    std::vector<uint16_t> colors; // BGR555
};

struct Tex0Data
{
    std::vector<DsTexture> textures;
    std::vector<DsPalette> palettes;
};

class NsbmdTex0Parser
{
public:
    static Tex0Data Parse(const uint8_t* data, size_t size);
};