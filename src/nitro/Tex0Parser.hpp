#pragma once

// ============================================================================
// Tex0Parser.hpp  —  NSBMD TEX0 Section Parser  (v0.3.0)
// ============================================================================
//
// Locates and parses the TEX0 section inside an NSBMD file.
// Produces a ParsedTex0 with raw texture metadata, pixel data, and palette
// data.  Actual pixel decoding to RGBA8 is handled by TextureDecoder.
// ============================================================================

#include <cstdint>
#include <filesystem>
#include <vector>

#include "nitro/NitroTexture.hpp"

namespace nitro
{
    class Tex0Parser
    {
    public:
        // Parse all texture/palette metadata from an NSBMD file.
        // Returns an invalid ParsedTex0 if the file has no TEX0 section.
        static ParsedTex0 parse(const std::filesystem::path& path);

        // Same but accepts already-loaded file bytes (avoids double-read when
        // the caller (e.g. geometry decoder) already has the data in memory).
        static ParsedTex0 parseFromBytes(const std::vector<std::uint8_t>& data);
    };

} // namespace nitro
