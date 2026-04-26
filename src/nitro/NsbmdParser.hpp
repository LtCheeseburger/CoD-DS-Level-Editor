#pragma once

#include <filesystem>

#include "nitro/NsbmdFile.hpp"

namespace nitro
{
    class NsbmdParser
    {
    public:
        static NsbmdFile parseHeaderOnly(const std::filesystem::path& path);
    };
}