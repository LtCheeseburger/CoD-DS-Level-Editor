#include "Tex0Parser.hpp"
#include <cstdio>

namespace nitro
{

ParsedTex0 Tex0Parser::parse(const std::vector<uint8_t>& data)
{
    ParsedTex0 parsed;

    // your existing parsing logic here...

    for (auto& tex : parsed.textures)
    {
        // 🔥 compute GX texture address
        uint32_t param = tex.texImageParam;

        tex.decoded.texAddr = (param & 0xFFFF) << 3;

        printf("[TEX0] %s → addr=0x%08X size=%ux%u\n",
            tex.decoded.name.c_str(),
            tex.decoded.texAddr,
            tex.decoded.width,
            tex.decoded.height);
    }

    return parsed;
}

}