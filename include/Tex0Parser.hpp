#pragma once
// =============================================================================
//  Tex0Parser.hpp
//  Parses the TEX0 section of an NSBMD file and produces NitroTexture objects.
//
//  TEX0 binary layout (offsets relative to TEX0 block start)
//  ----------------------------------------------------------
//  0x00  u16   data section size / 8  (texture image data)
//  0x02  u16   info offset
//  0x04  u32   reserved
//  0x08  u32   texData offset (from block start)
//  0x0C  u32   reserved
//  0x10  u16   plttData size / 8
//  0x12  u16   reserved
//  0x14  u32   plttData offset (from block start)
//  0x18  Dictionary (name table + texImageParam array)
//
//  Each dictionary entry gives: name (16 bytes) + texImageParam (4 bytes).
//  The texImageParam encodes: address, width, height, format, repeat, flip, etc.
//
//  The palette dictionary immediately follows the texture dictionary.
// =============================================================================

#include "NitroTexture.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// ParsedTex0
//  Result of a successful TEX0 parse.  Contains all decoded textures.
// ---------------------------------------------------------------------------
struct ParsedTex0
{
    std::vector<NitroTexture> textures; ///< One entry per TEX0 texture entry
    bool                      valid = false;
};

// ---------------------------------------------------------------------------
// Tex0Parser
//  Stateless utility class.  Call parse() once per NSBMD file load.
// ---------------------------------------------------------------------------
class Tex0Parser
{
public:
    Tex0Parser()  = default;
    ~Tex0Parser() = default;

    // Non-copyable, non-movable (stateless utility — no need to copy)
    Tex0Parser(const Tex0Parser&)            = delete;
    Tex0Parser& operator=(const Tex0Parser&) = delete;

    // -----------------------------------------------------------------------
    // parse()
    //   tex0Data  – the raw bytes of the TEX0 block (starting at the TEX0
    //               section header, NOT the NSBMD file header).
    //
    // Returns a ParsedTex0 with valid=true on success.
    // On failure, valid=false and textures is empty.
    // -----------------------------------------------------------------------
    [[nodiscard]] ParsedTex0 parse(std::span<const uint8_t> tex0Data) const;

    // -----------------------------------------------------------------------
    // decode()
    //   Fills tex.rgba8 for every texture that has isValid()==true.
    //   tex0Data must be the same buffer passed to parse().
    //   Call this after parse(); skip if you only need metadata.
    // -----------------------------------------------------------------------
    void decode(ParsedTex0& parsed, std::span<const uint8_t> tex0Data) const;

    // Public utility: decode a DS BGR555 colour to RGBA8 uint32 (R,G,B,A bytes).
    // opaque=true → alpha=255; opaque=false → alpha=0 (used for colour0 transparency).
    static uint32_t bgr555ToRgba8(uint16_t bgr555, bool opaque = true);

private:
    // Internal decoding helpers (one per format)
    void decodeA3I5      (NitroTexture& tex, std::span<const uint8_t> texData,
                          std::span<const uint8_t> plttData) const;
    void decodePalette4  (NitroTexture& tex, std::span<const uint8_t> texData,
                          std::span<const uint8_t> plttData) const;
    void decodePalette16 (NitroTexture& tex, std::span<const uint8_t> texData,
                          std::span<const uint8_t> plttData) const;
    void decodePalette256(NitroTexture& tex, std::span<const uint8_t> texData,
                          std::span<const uint8_t> plttData) const;
    void decodeCompressed(NitroTexture& tex, std::span<const uint8_t> texData,
                          std::span<const uint8_t> plttData) const;
    void decodeA5I3      (NitroTexture& tex, std::span<const uint8_t> texData,
                          std::span<const uint8_t> plttData) const;
    void decodeDirect    (NitroTexture& tex, std::span<const uint8_t> texData) const;
};
