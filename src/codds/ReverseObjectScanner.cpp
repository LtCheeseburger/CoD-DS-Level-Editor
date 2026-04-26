#include "ReverseObjectScanner.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

namespace codds
{
    namespace
    {
        bool canRead(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t bytes)
        {
            return offset <= data.size() && bytes <= data.size() - offset;
        }

        std::uint16_t readU16(const std::vector<std::uint8_t>& data, std::size_t offset)
        {
            return static_cast<std::uint16_t>(data[offset]) |
                   static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1]) << 8);
        }

        std::int16_t readS16(const std::vector<std::uint8_t>& data, std::size_t offset)
        {
            return static_cast<std::int16_t>(readU16(data, offset));
        }

        std::int32_t readS32(const std::vector<std::uint8_t>& data, std::size_t offset)
        {
            const std::uint32_t value =
                static_cast<std::uint32_t>(data[offset]) |
                (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
                (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
                (static_cast<std::uint32_t>(data[offset + 3]) << 24);
            return static_cast<std::int32_t>(value);
        }

        float readFloat32(const std::vector<std::uint8_t>& data, std::size_t offset)
        {
            float value = 0.0f;
            std::memcpy(&value, data.data() + offset, sizeof(float));
            return value;
        }

        std::size_t coordinateByteWidth(CoordinateFormat format)
        {
            switch (format)
            {
                case CoordinateFormat::Float32: return 4;
                case CoordinateFormat::Int16Fixed4096: return 2;
                case CoordinateFormat::Int32Fixed4096: return 4;
            }
            return 4;
        }

        float readCoordinate(const std::vector<std::uint8_t>& data, std::size_t offset, CoordinateFormat format)
        {
            switch (format)
            {
                case CoordinateFormat::Float32:
                    return readFloat32(data, offset);
                case CoordinateFormat::Int16Fixed4096:
                    return static_cast<float>(readS16(data, offset)) / 4096.0f;
                case CoordinateFormat::Int32Fixed4096:
                    return static_cast<float>(readS32(data, offset)) / 4096.0f;
            }
            return 0.0f;
        }

        bool plausible(float v)
        {
            return std::isfinite(v) && std::abs(v) <= 100000.0f;
        }

        bool plausibleRecordSize(std::uint16_t size, std::size_t remaining)
        {
            return size >= 8 && size <= 4096 && size <= remaining;
        }
    }

    std::vector<ReverseObjectInstance> ReverseObjectScanner::scanObjects(
        const std::filesystem::path& path,
        const ReverseLayoutSettings& settings,
        const ModelLibrary* modelLibrary)
    {
        std::vector<ReverseObjectInstance> out;

        try
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return out;

            std::vector<std::uint8_t> data{
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            };

            if (settings.startOffset >= data.size())
                return out;

            const std::size_t coordBytes = coordinateByteWidth(settings.coordinateFormat);
            const std::size_t maxFieldEnd = std::max({
                static_cast<std::size_t>(settings.xOffset) + coordBytes,
                static_cast<std::size_t>(settings.yOffset) + coordBytes,
                static_cast<std::size_t>(settings.zOffset) + coordBytes,
                static_cast<std::size_t>(settings.modelIndexOffset) + 2u,
                static_cast<std::size_t>(settings.recordSizeOffset) + 2u
            });

            std::size_t recordOffset = settings.startOffset;
            std::uint32_t recordIndex = 0;

            while (recordOffset < data.size() && recordIndex < settings.maxRecords)
            {
                std::size_t recordSize = settings.structSize;

                if (settings.objectsLpVariableRecordMode)
                {
                    const std::size_t sizeOff = recordOffset + settings.recordSizeOffset;
                    if (!canRead(data, sizeOff, 2))
                        break;

                    recordSize = readU16(data, sizeOff);
                    if (!plausibleRecordSize(static_cast<std::uint16_t>(recordSize), data.size() - recordOffset))
                    {
                        core::Logger::warning("Objects.LP variable scan stopped at invalid record size " +
                                              std::to_string(recordSize) + " at 0x" +
                                              std::to_string(recordOffset));
                        break;
                    }
                }
                else if (recordSize == 0)
                {
                    return out;
                }

                if (maxFieldEnd > recordSize)
                {
                    recordOffset += recordSize;
                    ++recordIndex;
                    continue;
                }

                const std::size_t xOff = recordOffset + settings.xOffset;
                const std::size_t yOff = recordOffset + settings.yOffset;
                const std::size_t zOff = recordOffset + settings.zOffset;
                const std::size_t modelOff = recordOffset + settings.modelIndexOffset;

                if (!canRead(data, xOff, coordBytes) || !canRead(data, yOff, coordBytes) ||
                    !canRead(data, zOff, coordBytes) || !canRead(data, modelOff, 2))
                {
                    break;
                }

                const float x = readCoordinate(data, xOff, settings.coordinateFormat);
                const float y = readCoordinate(data, yOff, settings.coordinateFormat);
                const float z = readCoordinate(data, zOff, settings.coordinateFormat);

                if (plausible(x) && plausible(y) && plausible(z))
                {
                    ReverseObjectInstance instance;
                    instance.x = x;
                    instance.y = y;
                    instance.z = z;
                    instance.recordIndex = recordIndex;
                    instance.fileOffset = static_cast<std::uint32_t>(recordOffset);
                    instance.modelIndex = readU16(data, modelOff);

                    if (modelLibrary)
                    {
                        if (const auto* model = modelLibrary->findByIndex(instance.modelIndex))
                        {
                            instance.modelResolved = true;
                            instance.modelName = model->displayName;
                            instance.modelPath = model->absolutePath;
                        }
                    }

                    out.push_back(instance);
                }

                recordOffset += recordSize;
                ++recordIndex;
            }

            core::Logger::info(std::string("Objects.LP ") +
                               (settings.objectsLpVariableRecordMode ? "variable" : "fixed") +
                               " cube scan: " + std::to_string(out.size()) + " object candidate(s)");
        }
        catch (const std::exception& e)
        {
            core::Logger::error(std::string("Objects.LP cube scan failed: ") + e.what());
        }

        return out;
    }
}
