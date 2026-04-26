#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "nitro/NitroTexture.hpp"

namespace nitro
{
    class Tex0Parser
    {
    public:
        static ParsedTex0 parse(const std::filesystem::path& path);
        static ParsedTex0 parseFromBytes(const std::vector<std::uint8_t>& data);
    };

} // namespace nitro
