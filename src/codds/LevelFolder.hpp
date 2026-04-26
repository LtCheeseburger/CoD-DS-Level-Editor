#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "codds/LevelFileRole.hpp"

namespace codds
{
    struct LevelFileEntry
    {
        std::filesystem::path path;
        LevelFileRole role = LevelFileRole::Unknown;
        std::uintmax_t sizeBytes = 0;
    };

    struct LevelFolder
    {
        std::filesystem::path rootPath;
        std::vector<LevelFileEntry> files;

        std::vector<LevelFileEntry> models;
        std::vector<LevelFileEntry> objectPlacement;
        std::vector<LevelFileEntry> roomLayout;
        std::vector<LevelFileEntry> collision;
        std::vector<LevelFileEntry> scripts;
        std::vector<LevelFileEntry> unknown;
    };

    class LevelFolderScanner
    {
    public:
        static LevelFolder scan(const std::filesystem::path& folderPath);
    };
}
