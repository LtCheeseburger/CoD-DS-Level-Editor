#pragma once

// ============================================================================
// NsbmdUvExtractor.hpp  —  GX TEXCOORD UV Extraction  (v0.3.0)
// ============================================================================
//
// Extracts per-vertex UV coordinates from a GX display list, producing a
// std::vector<TexCoord> that is parallel to DecodedNsbmdMesh::vertices.
//
// IMPORTANT: this file does NOT touch NsbmdGeometryDecoder. It runs its own
// lightweight GX command walker that mirrors the same packet protocol but
// records only TEXCOORD (0x22) and vertex commands needed to advance the
// vertex counter.
//
// GX TEXCOORD (0x22):
//   1 argument word:
//     bits [15:0]  s  — signed 12.4 fixed-point (1/16)
//     bits [31:16] t  — signed 12.4 fixed-point (1/16)
//
// The UV emitted before each vertex command applies to that vertex.
// The DS TEXCOORD command sets a "pending UV" that is consumed when the
// next VTX_* command fires.  If no TEXCOORD was seen before a vertex, the
// previous pending UV is reused (DS hardware latches the last value).
//
// Output UVs are normalized to [0,1] and V is flipped:
//   u = s / width
//   v = 1.0 - (t / height)     ← OpenGL bottom-left origin
// ============================================================================

#include <cstdint>
#include <filesystem>
#include <vector>

#include "nitro/NitroTexture.hpp"

namespace nitro
{
    struct DecodedNsbmdMesh;   // Forward declaration — defined in NsbmdGeometryDecoder.hpp

    class NsbmdUvExtractor
    {
    public:
        // Extract UV coordinates for every vertex in `mesh`.
        // texWidth/texHeight are needed to normalize the DS fixed-point UVs.
        // Returns a vector with mesh.vertices.size() entries (0,0 if unavailable).
        //
        // path: the same .nsbmd file that produced `mesh`.
        static std::vector<TexCoord> extractUVs(const std::filesystem::path& path,
                                                 const DecodedNsbmdMesh&      mesh,
                                                 std::uint16_t                texWidth,
                                                 std::uint16_t                texHeight);
    };

} // namespace nitro
