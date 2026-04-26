#include "Tex0Parser.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace nitro
{
    namespace
    {
        static std::uint16_t rd16(const std::uint8_t* p)
        {
            return static_cast<std::uint16_t>(p[0]) |
                   (static_cast<std::uint16_t>(p[1]) << 8);
        }

        static std::uint32_t rd32(const std::uint8_t* p)
        {
            return static_cast<std::uint32_t>(p[0]) |
                   (static_cast<std::uint32_t>(p[1]) << 8) |
                   (static_cast<std::uint32_t>(p[2]) << 16) |
                   (static_cast<std::uint32_t>(p[3]) << 24);
        }

        static std::uint32_t bytesPerTexture(std::uint16_t w, std::uint16_t h, NitroTexFmt fmt)
        {
            const std::uint32_t px = static_cast<std::uint32_t>(w) * h;
            switch (fmt)
            {
                case NitroTexFmt::Palette4:   return (px + 3u) / 4u;
                case NitroTexFmt::Palette16:  return (px + 1u) / 2u;
                case NitroTexFmt::Palette256:
                case NitroTexFmt::A3I5:
                case NitroTexFmt::A5I3:       return px;
                case NitroTexFmt::Tex4x4:     return px / 4u;
                case NitroTexFmt::Direct:     return px * 2u;
                default:                       return 0u;
            }
        }

        static std::uint32_t paletteColorsForFormat(NitroTexFmt fmt)
        {
            switch (fmt)
            {
                case NitroTexFmt::Palette4:   return 4;
                case NitroTexFmt::Palette16:  return 16;
                case NitroTexFmt::Palette256: return 256;
                case NitroTexFmt::A3I5:       return 32;
                case NitroTexFmt::A5I3:       return 8;
                default:                       return 0;
            }
        }

        static bool findTex0Block(const std::vector<std::uint8_t>& data,
                                  std::size_t& tex0Start,
                                  std::size_t& tex0Size)
        {
            tex0Start = 0;
            tex0Size  = 0;

            for (std::size_t i = 0; i + 8 <= data.size(); ++i)
            {
                if (data[i] == 'T' && data[i + 1] == 'E' && data[i + 2] == 'X' && data[i + 3] == '0')
                {
                    const std::uint32_t blockSize = rd32(data.data() + i + 4);
                    if (blockSize >= 0x20 && i + blockSize <= data.size())
                    {
                        tex0Start = i;
                        tex0Size  = blockSize;
                        return true;
                    }
                }
            }
            return false;
        }
    }

    ParsedTex0 Tex0Parser::parse(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};

        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return parseFromBytes(bytes);
    }

    ParsedTex0 Tex0Parser::parseFromBytes(const std::vector<std::uint8_t>& data)
    {
        ParsedTex0 parsed;

        std::size_t tex0Start = 0;
        std::size_t tex0Size  = 0;
        if (!findTex0Block(data, tex0Start, tex0Size))
            return parsed;

        const auto* base = data.data() + tex0Start;

        const std::uint32_t texDataSizeBytes = static_cast<std::uint32_t>(rd16(base + 0x00)) * 8u;
        const std::uint16_t infoOffset       = rd16(base + 0x02);
        const std::uint32_t texDataOffset    = rd32(base + 0x08);
        const std::uint32_t plttDataSize     = static_cast<std::uint32_t>(rd16(base + 0x10)) * 8u;
        const std::uint32_t plttDataOffset   = rd32(base + 0x14);

        if (infoOffset >= tex0Size)
            return parsed;

        if (texDataOffset < tex0Size)
        {
            const std::size_t available = tex0Size - texDataOffset;
            const std::size_t size = std::min<std::size_t>(texDataSizeBytes, available);
            parsed.textureData.assign(base + texDataOffset, base + texDataOffset + size);
        }

        if (plttDataOffset < tex0Size)
        {
            const std::size_t available = tex0Size - plttDataOffset;
            const std::size_t size = std::min<std::size_t>(plttDataSize, available);
            parsed.paletteData.assign(base + plttDataOffset, base + plttDataOffset + size);
        }

        const auto* info = base + infoOffset;
        const std::size_t infoSize = tex0Size - infoOffset;
        if (infoSize < 4)
            return parsed;

        const std::uint16_t texCount = rd16(info + 0x02);
        constexpr std::size_t kParamSize = 8;
        constexpr std::size_t kNameSize  = 16;

        if (texCount == 0)
        {
            parsed.valid = true;
            return parsed;
        }

        const std::size_t needed = 4 + static_cast<std::size_t>(texCount) * (kParamSize + kNameSize);
        if (needed > infoSize)
            return parsed;

        const auto* params = info + 0x04;
        const auto* names  = params + texCount * kParamSize;

        parsed.textures.reserve(texCount);

        for (std::uint16_t i = 0; i < texCount; ++i)
        {
            NitroRawTexture raw;

            char nameBuf[17] = {};
            std::memcpy(nameBuf, names + i * kNameSize, kNameSize);
            raw.name = nameBuf;

            const auto* p = params + i * kParamSize;
            const std::uint16_t texSizeShifted = rd16(p + 0x00);
            const std::uint16_t paletteOffset  = rd16(p + 0x02);
            const std::uint32_t texImageParam  = rd32(p + 0x04);

            raw.texImageParam = texImageParam;
            raw.texAddr = (texImageParam & 0xFFFFu) << 3u;
            raw.widthPx  = static_cast<std::uint16_t>(8u << ((texImageParam >> 20) & 0x7u));
            raw.heightPx = static_cast<std::uint16_t>(8u << ((texImageParam >> 23) & 0x7u));
            raw.format   = static_cast<NitroTexFmt>((texImageParam >> 26) & 0x7u);

            raw.dataOffset = raw.texAddr;
            raw.dataSize = bytesPerTexture(raw.widthPx, raw.heightPx, raw.format);
            if (raw.dataSize == 0 && texSizeShifted != 0)
                raw.dataSize = static_cast<std::uint32_t>(texSizeShifted) * 8u;

            raw.paletteDataOffset = static_cast<std::uint32_t>(paletteOffset) * 8u;
            raw.paletteColorCount = paletteColorsForFormat(raw.format);

            raw.color0Transparent = ((texImageParam >> 29) & 1u) != 0;
            raw.hasAlpha = raw.format == NitroTexFmt::A3I5
                        || raw.format == NitroTexFmt::A5I3
                        || raw.format == NitroTexFmt::Direct
                        || raw.format == NitroTexFmt::Tex4x4;

            raw.valid = (raw.widthPx > 0 && raw.heightPx > 0 && raw.format != NitroTexFmt::None);
            parsed.textures.push_back(raw);
        }

        parsed.valid = true;
        return parsed;
    }

} // namespace nitro
