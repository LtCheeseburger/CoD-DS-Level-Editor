#include "NsbmdParser.hpp"

#include "core/BinaryReader.hpp"
#include "core/Logger.hpp"

#include <stdexcept>

namespace nitro
{
    NsbmdFile NsbmdParser::parseHeaderOnly(const std::filesystem::path& path)
    {
        NsbmdFile out;
        out.path = path;

        try
        {
            auto reader = core::BinaryReader::fromFile(path, core::Endian::Little);

            out.magic = reader.readString(4);

            if (out.magic != "BMD0" && out.magic != "BDL0")
            {
                core::Logger::warning("Not an NSBMD/BDL file: " + path.string());
                return out;
            }

            out.byteOrder = reader.readU16();
            out.version = reader.readU16();
            out.fileSize = reader.readU32();
            out.headerSize = reader.readU16();
            out.sectionCount = reader.readU16();

            for (std::uint16_t i = 0; i < out.sectionCount; ++i)
            {
                NitroSection section;
                section.offset = reader.readU32();

                const auto current = reader.position();

                if (section.offset + 4 <= reader.size())
                {
                    reader.seek(section.offset);
                    section.tag = reader.readString(4);
                    reader.seek(current);
                }

                out.sections.push_back(section);
            }

            out.valid = true;

            core::Logger::info("Parsed NSBMD header: " + path.string());
        }
        catch (const std::exception& e)
        {
            core::Logger::error(std::string("NSBMD parse failed: ") + e.what());
        }

        return out;
    }
}