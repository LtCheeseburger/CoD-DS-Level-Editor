#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace core
{
    enum class Endian
    {
        Little,
        Big
    };

    class BinaryReader
    {
    public:
        BinaryReader() = default;
        explicit BinaryReader(std::vector<std::uint8_t> data, Endian endian = Endian::Little);

        static BinaryReader fromFile(const std::filesystem::path& path, Endian endian = Endian::Little);

        bool valid() const;
        bool eof() const;

        std::size_t position() const;
        std::size_t size() const;

        void seek(std::size_t offset);
        void skip(std::size_t amount);

        std::uint8_t  readU8();
        std::uint16_t readU16();
        std::uint32_t readU32();

        std::int16_t readS16();
        std::int32_t readS32();

        std::string readString(std::size_t length);

    private:
        template <typename T>
        T readIntegral();

        std::vector<std::uint8_t> m_data;
        std::size_t m_position = 0;
        Endian m_endian = Endian::Little;
    };
}