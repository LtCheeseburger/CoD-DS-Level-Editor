#include "ModelLibrary.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <cctype>

namespace codds
{
    void ModelLibrary::scan(const std::filesystem::path& rootPath)
    {
        clear();
        m_rootPath = std::filesystem::absolute(rootPath);

        if (!std::filesystem::exists(m_rootPath) || !std::filesystem::is_directory(m_rootPath))
        {
            core::Logger::warning("ModelLibrary root is not a directory: " + m_rootPath.string());
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(m_rootPath))
        {
            if (!entry.is_regular_file())
                continue;

            const auto& path = entry.path();
            if (lowercase(path.extension().string()) != ".nsbmd")
                continue;

            ModelAsset asset;
            asset.displayName = path.filename().string();
            asset.absolutePath = std::filesystem::absolute(path);
            asset.relativePath = std::filesystem::relative(asset.absolutePath, m_rootPath);
            asset.lookupKey = lowercase(asset.displayName);

            m_assets.push_back(std::move(asset));
        }

        std::sort(m_assets.begin(), m_assets.end(), [](const ModelAsset& a, const ModelAsset& b) {
            return a.lookupKey < b.lookupKey;
        });

        core::Logger::info("ModelLibrary indexed " + std::to_string(m_assets.size()) + " NSBMD model(s)");
    }

    void ModelLibrary::clear()
    {
        m_rootPath.clear();
        m_assets.clear();
    }

    const std::vector<ModelAsset>& ModelLibrary::assets() const
    {
        return m_assets;
    }

    const ModelAsset* ModelLibrary::findByName(const std::string& name) const
    {
        const auto key = lowercase(name);
        for (const auto& asset : m_assets)
        {
            if (asset.lookupKey == key)
                return &asset;
        }
        return nullptr;
    }

    const ModelAsset* ModelLibrary::findByIndex(std::size_t index) const
    {
        if (index >= m_assets.size())
            return nullptr;
        return &m_assets[index];
    }

    const ModelAsset* ModelLibrary::findByStem(const std::string& stem) const
    {
        const auto key = lowercase(stem);
        for (const auto& asset : m_assets)
        {
            if (lowercase(asset.absolutePath.stem().string()) == key)
                return &asset;
        }
        return nullptr;
    }

    const std::filesystem::path& ModelLibrary::rootPath() const
    {
        return m_rootPath;
    }

    std::string ModelLibrary::lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }
}
