#include "NsbmdUvExtractor.hpp"

#include "nitro/NsbmdGeometryDecoder.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

// ============================================================================
// NsbmdUvExtractor.cpp  —  GX TEXCOORD UV Extraction  (v0.3.0)
// ============================================================================
//
// Mirrors only the packet-unpacking and vertex-counting logic of the geometry
// decoder.  Does NOT modify any geometry state.
// ============================================================================

namespace nitro
{
    namespace
    {
        // ------------------------------------------------------------------ //
        //  Byte helpers (same as geometry decoder, local copy)                //
        // ------------------------------------------------------------------ //

        bool canRead(const std::vector<std::uint8_t>& d, std::size_t off, std::size_t len)
        {
            return off <= d.size() && len <= d.size() - off;
        }

        std::uint16_t rU16(const std::vector<std::uint8_t>& d, std::size_t o)
        {
            return static_cast<std::uint16_t>(d[o]) | (static_cast<std::uint16_t>(d[o+1]) << 8);
        }

        std::uint32_t rU32(const std::vector<std::uint8_t>& d, std::size_t o)
        {
            return static_cast<std::uint32_t>(d[o])
                 | (static_cast<std::uint32_t>(d[o+1]) <<  8)
                 | (static_cast<std::uint32_t>(d[o+2]) << 16)
                 | (static_cast<std::uint32_t>(d[o+3]) << 24);
        }

        std::int16_t rS16(const std::vector<std::uint8_t>& d, std::size_t o)
        {
            return static_cast<std::int16_t>(rU16(d, o));
        }

        // ------------------------------------------------------------------ //
        //  Argument word count (mirrors geometry decoder exactly)             //
        // ------------------------------------------------------------------ //

        int commandArgWords(std::uint8_t cmd)
        {
            switch (cmd)
            {
                case 0x00: return 0;
                case 0x10: return 1;   // MTX_MODE
                case 0x11: return 0;   // MTX_PUSH
                case 0x12: return 1;   // MTX_POP
                case 0x13: return 1;   // MTX_STORE
                case 0x14: return 1;   // MTX_RESTORE
                case 0x15: return 0;   // MTX_IDENTITY
                case 0x16: return 16;  // MTX_LOAD_4x4
                case 0x17: return 12;  // MTX_LOAD_4x3
                case 0x18: return 16;  // MTX_MULT_4x4
                case 0x19: return 12;  // MTX_MULT_4x3
                case 0x1A: return 9;   // MTX_MULT_3x3
                case 0x1B: return 3;   // MTX_SCALE
                case 0x1C: return 3;   // MTX_TRANS
                case 0x20: return 1;   // COLOR
                case 0x21: return 1;   // NORMAL
                case 0x22: return 1;   // TEXCOORD  ← we process this
                case 0x23: return 2;   // VTX_16
                case 0x24: return 1;   // VTX_10
                case 0x25: return 1;   // VTX_XY
                case 0x26: return 1;   // VTX_XZ
                case 0x27: return 1;   // VTX_YZ
                case 0x28: return 1;   // VTX_DIFF
                case 0x29: return 1;   // POLYGON_ATTR
                case 0x2A: return 1;   // TEXIMAGE_PARAM
                case 0x2B: return 1;   // PLTT_BASE
                case 0x30: return 1;   // DIF_AMB
                case 0x31: return 1;   // SPE_EMI
                case 0x32: return 1;   // LIGHT_VECTOR
                case 0x33: return 1;   // LIGHT_COLOR
                case 0x34: return 32;  // SHININESS
                case 0x40: return 1;   // BEGIN_VTXS
                case 0x41: return 0;   // END_VTXS
                case 0x50: return 1;   // SWAP_BUFFERS
                case 0x60: return 1;   // VIEWPORT
                case 0x70: return 3;   // BOX_TEST
                case 0x71: return 2;   // POS_TEST
                case 0x72: return 1;   // VEC_TEST
                default:   return -1;
            }
        }

        bool isVertexCmd(std::uint8_t cmd)
        {
            return cmd >= 0x23 && cmd <= 0x28;
        }

        // ------------------------------------------------------------------ //
        //  MDL0 structure helpers (mirrors geometry decoder)                  //
        // ------------------------------------------------------------------ //

        struct DlRegion { std::size_t start, size; };

        std::vector<std::size_t> parseDictionary(const std::vector<std::uint8_t>& d,
                                                  std::size_t dictAbs)
        {
            std::vector<std::size_t> result;
            if (!canRead(d, dictAbs, 8)) return result;
            const std::uint32_t count = rU32(d, dictAbs);
            if (count == 0 || count > 256) return result;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const std::size_t entBase = dictAbs + 8u + static_cast<std::size_t>(i) * 8u;
                if (!canRead(d, entBase, 8)) break;
                result.push_back(dictAbs + rU32(d, entBase + 4u));
            }
            return result;
        }

        std::vector<DlRegion> extractDlRegions(const std::vector<std::uint8_t>& d,
                                                std::size_t mdl0Off, std::size_t mdl0End)
        {
            std::vector<DlRegion> regions;
            if (!canRead(d, mdl0Off, 12)) return regions;
            const std::size_t modelTableAbs = mdl0Off + rU32(d, mdl0Off + 8u);
            for (std::size_t modelAbs : parseDictionary(d, modelTableAbs))
            {
                if (!canRead(d, modelAbs, 0x40)) continue;
                const std::size_t polyTableAbs = modelAbs + rU32(d, modelAbs + 0x38u);
                for (std::size_t polyAbs : parseDictionary(d, polyTableAbs))
                {
                    if (!canRead(d, polyAbs, 8)) continue;
                    const std::uint32_t dlRel  = rU32(d, polyAbs);
                    const std::uint32_t dlSize = rU32(d, polyAbs + 4u);
                    if (dlSize == 0 || dlSize > 0x200000u) continue;
                    const std::size_t dlAbs = polyAbs + dlRel;
                    if (dlAbs + dlSize > d.size()) continue;
                    if (dlAbs < mdl0Off || dlAbs >= mdl0End) continue;
                    regions.push_back({dlAbs, dlSize});
                }
            }
            return regions;
        }

        // ------------------------------------------------------------------ //
        //  UV walk state                                                       //
        // ------------------------------------------------------------------ //

        struct UvWalkState
        {
            float pendingU = 0.0f;
            float pendingV = 0.0f;  // DS-space, not yet flipped
            std::size_t vertexIndex = 0;
        };

        // Run the display list, collecting one TexCoord per vertex command.
        // vertexCount: expected number of vertices (from DecodedNsbmdMesh).
        void walkDl(const std::vector<std::uint8_t>& d,
                    std::size_t start, std::size_t end,
                    float invW, float invH,
                    std::vector<TexCoord>& outUVs,
                    std::size_t vertexCount)
        {
            UvWalkState state;
            std::size_t pc = start;

            while (pc + 4 <= end && state.vertexIndex < vertexCount)
            {
                const std::uint32_t packed = rU32(d, pc);
                pc += 4;

                const std::uint8_t cmds[4] = {
                    static_cast<std::uint8_t>( packed        & 0xFFu),
                    static_cast<std::uint8_t>((packed >>  8) & 0xFFu),
                    static_cast<std::uint8_t>((packed >> 16) & 0xFFu),
                    static_cast<std::uint8_t>((packed >> 24) & 0xFFu)
                };

                if (!cmds[0] && !cmds[1] && !cmds[2] && !cmds[3]) continue;

                for (std::uint8_t cmd : cmds)
                {
                    if (cmd == 0x00) continue;

                    const int words = commandArgWords(cmd);
                    if (words < 0) continue;
                    if (!canRead(d, pc, static_cast<std::size_t>(words) * 4u)) return;

                    if (cmd == 0x22) // TEXCOORD
                    {
                        const std::uint32_t w = rU32(d, pc);
                        // s and t are signed 12.4 fixed-point (1/16 units)
                        const std::int16_t s = static_cast<std::int16_t>( w        & 0xFFFFu);
                        const std::int16_t t = static_cast<std::int16_t>((w >> 16) & 0xFFFFu);

                        state.pendingU = static_cast<float>(s) / 16.0f * invW;
                        // DS V increases downward; flip to OpenGL (1 - v)
                        state.pendingV = 1.0f - (static_cast<float>(t) / 16.0f * invH);
                    }
                    else if (isVertexCmd(cmd))
                    {
                        if (state.vertexIndex < vertexCount)
                        {
                            outUVs[state.vertexIndex] = { state.pendingU, state.pendingV };
                            ++state.vertexIndex;
                        }
                    }

                    pc += static_cast<std::size_t>(words) * 4u;
                }
            }
        }

        std::vector<std::uint8_t> readAll(const std::filesystem::path& path)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f) throw std::runtime_error("failed to open " + path.string());
            return { std::istreambuf_iterator<char>(f), {} };
        }

    } // anonymous namespace

    // ======================================================================= //
    //  Public API                                                              //
    // ======================================================================= //

    std::vector<TexCoord> NsbmdUvExtractor::extractUVs(const std::filesystem::path& path,
                                                        const DecodedNsbmdMesh&      mesh,
                                                        std::uint16_t                texWidth,
                                                        std::uint16_t                texHeight)
    {
        const std::size_t vtxCount = mesh.vertices.size();
        std::vector<TexCoord> uvs(vtxCount, {0.0f, 0.0f});

        if (vtxCount == 0 || texWidth == 0 || texHeight == 0)
            return uvs;

        try
        {
            const auto data = readAll(path);
            if (data.size() < 0x20) return uvs;

            const std::string magic(reinterpret_cast<const char*>(data.data()), 4);
            if (magic != "BMD0" && magic != "BDL0") return uvs;

            const float invW = 1.0f / static_cast<float>(texWidth);
            const float invH = 1.0f / static_cast<float>(texHeight);

            const std::uint16_t sectionCount = rU16(data, 0x0Eu);
            std::size_t mdl0Off = std::string::npos;
            std::size_t mdl0End = data.size();

            for (std::uint16_t i = 0; i < sectionCount; ++i)
            {
                const std::size_t off = 0x10u + static_cast<std::size_t>(i) * 4u;
                if (!canRead(data, off, 4)) break;
                const std::uint32_t secOff = rU32(data, off);
                if (!canRead(data, secOff, 4)) continue;
                if (std::string(reinterpret_cast<const char*>(data.data() + secOff), 4) == "MDL0")
                {
                    mdl0Off = secOff;
                    for (std::uint16_t j = 0; j < sectionCount; ++j)
                    {
                        const std::size_t o2 = 0x10u + static_cast<std::size_t>(j) * 4u;
                        if (!canRead(data, o2, 4)) break;
                        const std::uint32_t o2v = rU32(data, o2);
                        if (o2v > mdl0Off)
                            mdl0End = std::min(mdl0End, static_cast<std::size_t>(o2v));
                    }
                    break;
                }
            }

            if (mdl0Off == std::string::npos) return uvs;

            auto regions = extractDlRegions(data, mdl0Off, mdl0End);
            if (regions.empty()) return uvs;

            // Use the first (and typically only) DL region.
            // If the mesh came from the best-scoring region, we walk all of them
            // and pick the one that fills the most UV slots.
            std::vector<TexCoord> bestUVs = uvs;
            std::size_t           bestFilled = 0;

            for (const DlRegion& r : regions)
            {
                std::vector<TexCoord> candidate(vtxCount, {0.0f, 0.0f});
                walkDl(data, r.start, r.start + r.size, invW, invH, candidate, vtxCount);

                // Count non-zero UVs as "filled"
                std::size_t filled = 0;
                for (const TexCoord& tc : candidate)
                    if (tc.u != 0.0f || tc.v != 0.0f) ++filled;

                if (filled > bestFilled)
                {
                    bestFilled = filled;
                    bestUVs    = candidate;
                }
            }

            core::Logger::info(
                "NsbmdUvExtractor: extracted " + std::to_string(bestFilled) +
                "/" + std::to_string(vtxCount) + " non-zero UVs from " + path.filename().string());

            return bestUVs;
        }
        catch (const std::exception& e)
        {
            core::Logger::error(std::string("NsbmdUvExtractor failed: ") + e.what());
            return uvs;
        }
    }

} // namespace nitro
