#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codds
{
    struct ModelAsset
    {
        std::string displayName;
        std::filesystem::path relativePath;
        std::filesystem::path absolutePath;
        std::string lookupKey;
    };

    class ModelLibrary
    {
    public:
        void scan(const std::filesystem::path& rootPath);
        void clear();

        const std::vector<ModelAsset>& assets() const;

        const ModelAsset* findByName(const std::string& name) const;
        const ModelAsset* findByStem(const std::string& stem) const;
        const ModelAsset* findByIndex(std::size_t index) const;

        const std::filesystem::path& rootPath() const;

    private:
        static std::string lowercase(std::string value);

        std::filesystem::path m_rootPath;
        std::vector<ModelAsset> m_assets;
    };
}
