#include "BinaryReader.hpp"

#include <fstream>
#include <stdexcept>

namespace core
{
    BinaryReader::BinaryReader(std::vector<std::uint8_t> data, Endian endian)
        : m_data(std::move(data)), m_endian(endian)
    {
    }

    BinaryReader BinaryReader::fromFile(const std::filesystem::path& path, Endian endian)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file)
            throw std::runtime_error("Failed to open file: " + path.string());

        std::vector<std::uint8_t> data{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        return BinaryReader(std::move(data), endian);
    }

    bool BinaryReader::valid() const
    {
        return !m_data.empty();
    }

    bool BinaryReader::eof() const
    {
        return m_position >= m_data.size();
    }

    std::size_t BinaryReader::position() const
    {
        return m_position;
    }

    std::size_t BinaryReader::size() const
    {
        return m_data.size();
    }

    void BinaryReader::seek(std::size_t offset)
    {
        if (offset > m_data.size())
            throw std::runtime_error("BinaryReader seek out of bounds");

        m_position = offset;
    }

    void BinaryReader::skip(std::size_t amount)
    {
        seek(m_position + amount);
    }

    std::uint8_t BinaryReader::readU8()
    {
        return readIntegral<std::uint8_t>();
    }

    std::uint16_t BinaryReader::readU16()
    {
        return readIntegral<std::uint16_t>();
    }

    std::uint32_t BinaryReader::readU32()
    {
        return readIntegral<std::uint32_t>();
    }

    std::int16_t BinaryReader::readS16()
    {
        return static_cast<std::int16_t>(readU16());
    }

    std::int32_t BinaryReader::readS32()
    {
        return static_cast<std::int32_t>(readU32());
    }

    std::string BinaryReader::readString(std::size_t length)
    {
        if (m_position + length > m_data.size())
            throw std::runtime_error("BinaryReader string read out of bounds");

        std::string value(
            reinterpret_cast<const char*>(m_data.data() + m_position),
            length
        );

        m_position += length;
        return value;
    }

    template <typename T>
    T BinaryReader::readIntegral()
    {
        if (m_position + sizeof(T) > m_data.size())
            throw std::runtime_error("BinaryReader integral read out of bounds");

        T value = 0;

        if (m_endian == Endian::Little)
        {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                value |= static_cast<T>(m_data[m_position + i]) << (8 * i);
        }
        else
        {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                value = static_cast<T>((value << 8) | m_data[m_position + i]);
        }

        m_position += sizeof(T);
        return value;
    }
}