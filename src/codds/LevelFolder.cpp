#include "LevelFolder.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <stdexcept>

namespace codds
{
    namespace
    {
        void addToRoleBucket(LevelFolder& level, const LevelFileEntry& entry)
        {
            switch (entry.role)
            {
                case LevelFileRole::ModelAsset_NSBMD:
                    level.models.push_back(entry);
                    break;
                case LevelFileRole::ObjectPlacement_LP:
                    level.objectPlacement.push_back(entry);
                    break;
                case LevelFileRole::RoomLayout_ROO:
                    level.roomLayout.push_back(entry);
                    break;
                case LevelFileRole::Collision_CVHM:
                    level.collision.push_back(entry);
                    break;
                case LevelFileRole::Script_NSLBC:
                    level.scripts.push_back(entry);
                    break;
                case LevelFileRole::Unknown:
                    level.unknown.push_back(entry);
                    break;
            }
        }

        void sortEntries(std::vector<LevelFileEntry>& entries)
        {
            std::sort(entries.begin(), entries.end(), [](const LevelFileEntry& a, const LevelFileEntry& b) {
                return a.path.filename().string() < b.path.filename().string();
            });
        }
    }

    LevelFolder LevelFolderScanner::scan(const std::filesystem::path& folderPath)
    {
        if (!std::filesystem::exists(folderPath))
            throw std::runtime_error("Level folder does not exist: " + folderPath.string());

        if (!std::filesystem::is_directory(folderPath))
            throw std::runtime_error("Path is not a folder: " + folderPath.string());

        LevelFolder level;
        level.rootPath = folderPath;

        for (const auto& item : std::filesystem::directory_iterator(folderPath))
        {
            if (!item.is_regular_file())
                continue;

            LevelFileEntry entry;
            entry.path = item.path();
            entry.role = classifyLevelFile(item.path());
            entry.sizeBytes = item.file_size();

            level.files.push_back(entry);
            addToRoleBucket(level, entry);
        }

        sortEntries(level.files);
        sortEntries(level.models);
        sortEntries(level.objectPlacement);
        sortEntries(level.roomLayout);
        sortEntries(level.collision);
        sortEntries(level.scripts);
        sortEntries(level.unknown);

        core::Logger::info("Scanned CoD DS level folder: " + folderPath.string());
        return level;
    }
}
