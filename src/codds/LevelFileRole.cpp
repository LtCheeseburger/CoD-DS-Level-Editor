#include "LevelFileRole.hpp"

#include <algorithm>
#include <cctype>

namespace codds
{
    namespace
    {
        std::string lowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }
    }

    const char* toString(LevelFileRole role)
    {
        switch (role)
        {
            case LevelFileRole::ModelAsset_NSBMD:   return "Model Asset (NSBMD)";
            case LevelFileRole::ObjectPlacement_LP: return "Object Placement (LP)";
            case LevelFileRole::RoomLayout_ROO:     return "Room Layout (ROO)";
            case LevelFileRole::Collision_CVHM:     return "Collision / Height Mesh (CVHM)";
            case LevelFileRole::Script_NSLBC:       return "Script Bytecode (NSLBC)";
            case LevelFileRole::Unknown:            return "Unknown";
        }

        return "Unknown";
    }

    LevelFileRole classifyLevelFile(const std::filesystem::path& path)
    {
        const std::string filename = lowerCopy(path.filename().string());
        const std::string ext = lowerCopy(path.extension().string());

        if (ext == ".nsbmd" || ext == ".bmd" || ext == ".bdl")
            return LevelFileRole::ModelAsset_NSBMD;

        if (filename == "objects.lp" || ext == ".lp")
            return LevelFileRole::ObjectPlacement_LP;

        if (filename == "global.roo" || ext == ".roo")
            return LevelFileRole::RoomLayout_ROO;

        if (ext == ".cvhm")
            return LevelFileRole::Collision_CVHM;

        if (filename == "script.nslbc" || ext == ".nslbc")
            return LevelFileRole::Script_NSLBC;

        return LevelFileRole::Unknown;
    }
}
