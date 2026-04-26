#pragma once

#include <filesystem>
#include <string>

namespace codds
{
    enum class LevelFileRole
    {
        Unknown,
        ModelAsset_NSBMD,
        ObjectPlacement_LP,
        RoomLayout_ROO,
        Collision_CVHM,
        Script_NSLBC
    };

    const char* toString(LevelFileRole role);
    LevelFileRole classifyLevelFile(const std::filesystem::path& path);
}
