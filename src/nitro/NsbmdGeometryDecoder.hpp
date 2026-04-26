#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <QVector2D>
#include <QVector3D>

#include "GxTextureState.hpp"

namespace nitro
{
    struct DecodedNsbmdEdge
    {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
    };

    struct DecodedNsbmdMesh
    {
        bool valid = false;
        bool experimental = true;

        std::vector<QVector3D> vertices;
        std::vector<QVector2D> uvs;

        // Triangle list (a,b,c ...)
        std::vector<std::uint32_t> indices;

        std::vector<DecodedNsbmdEdge> edges;

        QVector3D minBounds {0.0f, 0.0f, 0.0f};
        QVector3D maxBounds {0.0f, 0.0f, 0.0f};

        // Debug / stats
        std::uint32_t commandWordsSeen = 0;
        std::uint32_t beginCommands = 0;
        std::uint32_t endCommands = 0;
        std::uint32_t vertexCommands = 0;
        std::uint32_t matrixCommands = 0;
        std::uint32_t unsupportedCommands = 0;

        std::uint32_t trianglePrimitives = 0;
        std::uint32_t quadPrimitives = 0;
        std::uint32_t triangleStripPrimitives = 0;
        std::uint32_t quadStripPrimitives = 0;

        std::string diagnostics;

        // Texture key per vertex
        std::vector<std::uint32_t> vertexTextureAddr;
    };

    class NsbmdGeometryDecoder
    {
    public:
        static DecodedNsbmdMesh decodeWireframeMesh(const std::filesystem::path& path);
    };

} // namespace nitro
