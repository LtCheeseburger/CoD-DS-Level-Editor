#include "DsTextureDecoder.h"
#include "DsPalette.h"

DecodedTexture DsTextureDecoder::Decode(
    const DsTexture& tex,
    const uint8_t* data,
    const DsPalette& pal)
{
    DecodedTexture out;
    out.width = tex.width;
    out.height = tex.height;
    out.rgba.resize(tex.width * tex.height * 4);

    for (int i = 0; i < tex.width * tex.height; i++)
    {
        uint8_t index = data[i];

        if (index >= pal.colors.size())
            index = 0;

        uint8_t r,g,b,a;
        BGR555ToRGBA(pal.colors[index], r,g,b,a);

        out.rgba[i*4+0] = r;
        out.rgba[i*4+1] = g;
        out.rgba[i*4+2] = b;
        out.rgba[i*4+3] = a;
    }

    return out;
}