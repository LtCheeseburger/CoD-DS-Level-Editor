#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace codds
{
    enum class CoordinateFormat
    {
        Float32,
        Int16Fixed4096,
        Int32Fixed4096
    };

    struct ReverseLayoutSettings
    {
        std::uint32_t startOffset = 0;
        std::uint32_t structSize = 32;
        std::uint32_t xOffset = 0;
        std::uint32_t yOffset = 4;
        std::uint32_t zOffset = 8;
        CoordinateFormat coordinateFormat = CoordinateFormat::Float32;
        std::uint32_t maxRecords = 2048;
        bool pointPreviewEnabled = false;
        bool normalizeScale = true;
        bool drawGrid = true;
        bool drawStrideLines = false;
        bool scanEntireLevelFolder = false;
        bool spawnObjectCubes = false;
        bool renderResolvedModels = false;
        bool objectsLpVariableRecordMode = false;
        std::uint32_t recordSizeOffset = 0;
        std::uint32_t modelIndexOffset = 0;
        float objectCubeSize = 0.35f;
        float scaleMultiplier = 1.0f;
    };

    struct ReversePoint
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        std::uint32_t recordIndex = 0;
        std::uint32_t fileOffset = 0;
    };

    struct ReverseObjectInstance
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        std::uint32_t recordIndex = 0;
        std::uint32_t fileOffset = 0;
        std::uint16_t modelIndex = 0;
        bool modelResolved = false;
        std::string modelName;
        std::filesystem::path modelPath;
    };
}
