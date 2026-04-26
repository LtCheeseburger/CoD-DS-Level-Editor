#include "ReversePointScanner.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace codds
{
    namespace
    {
        std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                throw std::runtime_error("Failed to open file: " + path.string());

            return std::vector<std::uint8_t>{
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            };
        }

        bool canRead(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t bytes)
        {
            return offset <= data.size() && bytes <= data.size() - offset;
        }

        std::uint16_t readU16LE(const std::vector<std::uint8_t>& data, std::size_t offset)
        {
            return static_cast<std::uint16_t>(data[offset]) |
                   static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1]) << 8);
        }

        std::uint32_t readU32LE(const std::vector<std::uint8_t>& data, std::size_t offset)
        {
            return static_cast<std::uint32_t>(data[offset]) |
                   (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
                   (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
                   (static_cast<std::uint32_t>(data[offset + 3]) << 24);
        }

        float readFloat32LE(const std::vector<std::uint8_t>& data, std::size_t offset)
        {
            const std::uint32_t raw = readU32LE(data, offset);
            float value = 0.0f;
            std::memcpy(&value, &raw, sizeof(float));
            return value;
        }

        float readCoordinate(const std::vector<std::uint8_t>& data, std::size_t offset, CoordinateFormat format)
        {
            switch (format)
            {
                case CoordinateFormat::Float32:
                    return readFloat32LE(data, offset);
                case CoordinateFormat::Int16Fixed4096:
                    return static_cast<float>(static_cast<std::int16_t>(readU16LE(data, offset))) / 4096.0f;
                case CoordinateFormat::Int32Fixed4096:
                    return static_cast<float>(static_cast<std::int32_t>(readU32LE(data, offset))) / 4096.0f;
            }
            return 0.0f;
        }

        std::size_t coordinateSize(CoordinateFormat format)
        {
            return format == CoordinateFormat::Int16Fixed4096 ? 2u : 4u;
        }

        bool validPoint(float x, float y, float z)
        {
            return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
                   std::abs(x) <= 100000.0f &&
                   std::abs(y) <= 100000.0f &&
                   std::abs(z) <= 100000.0f;
        }

        bool plausibleRecordSize(std::uint16_t size, std::size_t remaining)
        {
            return size >= 8 && size <= 4096 && size <= remaining;
        }
    }

    std::vector<ReversePoint> ReversePointScanner::scanPoints(
        const std::filesystem::path& path,
        const ReverseLayoutSettings& settings
    )
    {
        std::vector<ReversePoint> points;

        if (!settings.objectsLpVariableRecordMode && settings.structSize == 0)
            return points;

        try
        {
            const auto data = readAllBytes(path);
            if (settings.startOffset >= data.size())
                return points;

            const std::size_t componentSize = coordinateSize(settings.coordinateFormat);
            const std::size_t maxFieldEnd = std::max({
                static_cast<std::size_t>(settings.xOffset) + componentSize,
                static_cast<std::size_t>(settings.yOffset) + componentSize,
                static_cast<std::size_t>(settings.zOffset) + componentSize,
                static_cast<std::size_t>(settings.recordSizeOffset) + 2u
            });

            std::uint32_t recordIndex = 0;
            std::size_t base = settings.startOffset;

            while (base < data.size() && recordIndex < settings.maxRecords)
            {
                std::size_t recordSize = settings.structSize;
                if (settings.objectsLpVariableRecordMode)
                {
                    const std::size_t sizeOff = base + settings.recordSizeOffset;
                    if (!canRead(data, sizeOff, 2))
                        break;

                    recordSize = readU16LE(data, sizeOff);
                    if (!plausibleRecordSize(static_cast<std::uint16_t>(recordSize), data.size() - base))
                        break;
                }

                if (recordSize == 0 || maxFieldEnd > recordSize)
                {
                    if (!settings.objectsLpVariableRecordMode)
                        break;
                    base += recordSize == 0 ? 1 : recordSize;
                    ++recordIndex;
                    continue;
                }

                const float x = readCoordinate(data, base + settings.xOffset, settings.coordinateFormat);
                const float y = readCoordinate(data, base + settings.yOffset, settings.coordinateFormat);
                const float z = readCoordinate(data, base + settings.zOffset, settings.coordinateFormat);

                if (validPoint(x, y, z))
                {
                    ReversePoint point;
                    point.x = x;
                    point.y = y;
                    point.z = z;
                    point.recordIndex = recordIndex;
                    point.fileOffset = static_cast<std::uint32_t>(base);
                    points.push_back(point);
                }

                base += recordSize;
                ++recordIndex;
            }

            core::Logger::info("ReversePointScanner found " + std::to_string(points.size()) +
                               " candidate point(s) in " + path.filename().string());
        }
        catch (const std::exception& e)
        {
            core::Logger::error(std::string("ReversePointScanner failed: ") + e.what());
        }

        return points;
    }
}
