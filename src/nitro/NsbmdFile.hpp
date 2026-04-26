#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nitro
{
    struct NitroSection
    {
        std::string tag;
        std::uint32_t offset = 0;
    };

    struct NsbmdFile
    {
        std::filesystem::path path;

        bool valid = false;

        std::string magic;
        std::uint16_t byteOrder = 0;
        std::uint16_t version = 0;
        std::uint32_t fileSize = 0;
        std::uint16_t headerSize = 0;
        std::uint16_t sectionCount = 0;

        std::vector<NitroSection> sections;
    };
}