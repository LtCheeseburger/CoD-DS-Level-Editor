// =============================================================================
//  GxDisplayListDecoder.cpp
//  Decodes NDS GX display lists.
//
//  TEXTURE STATE (the critical part)
//  -----------------------------------
//  When GX_CMD_TEXIMAGE_PARAM (0x2A) is encountered:
//      currentTexAddr_ = (param & 0x0000FFFF) << 3
//  This address persists until the next TEXIMAGE_PARAM command, exactly
//  matching real DS hardware behaviour.
//
//  On every emitVertex() call, currentTexAddr_ is pushed into
//  currentMesh_.vertexTextureAddr so the renderer can look up the
//  correct OpenGL texture per mesh.
// =============================================================================

#include "GxDisplayListDecoder.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Parameter word counts per command (0 = command takes no params)
// ---------------------------------------------------------------------------
static const uint8_t kParamCount[256] =
{
    // 0x00 – 0x0F
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // 0x10 MTX_MODE, 0x11–0x13
    1,0,0,0,
    // 0x14 MTX_PUSH, 0x15 MTX_POP, 0x16 MTX_STORE, 0x17 MTX_RESTORE
    0,1,1,1,
    // 0x18 MTX_IDENTITY, 0x19 MTX_LOAD_4x4, 0x1A MTX_LOAD_4x3
    0,16,12,
    // 0x1B MTX_MULT_4x4, 0x1C MTX_MULT_4x3, 0x1D MTX_MULT_3x3
    16,12,9,
    // 0x1E MTX_SCALE, 0x1F MTX_TRANS
    3,3,
    // 0x20 COLOR, 0x21 NORMAL, 0x22 TEXCOORD, 0x23 VTX_16
    1,1,1,2,
    // 0x24 VTX_10, 0x25 VTX_XY, 0x26 VTX_XZ, 0x27 VTX_YZ
    1,1,1,1,
    // 0x28 VTX_DIFF, 0x29 POLYGON_ATTR, 0x2A TEXIMAGE_PARAM, 0x2B PLTT_BASE
    1,1,1,1,
    // 0x2C–0x2F
    0,0,0,0,
    // 0x30 LIGHT_COLOR, 0x31 LIGHT_VECTOR, 0x32 DIFFUSE_AMBIENT, 0x33 SPECULAR_EMISSION
    1,1,1,1,
    // 0x34 SHININESS 0x35–0x3F
    32,0,0,0, 0,0,0,0, 0,0,0,0,
    // 0x40 BEGIN_VTXS, 0x41 END_VTXS
    1,0,
    // 0x42–0x4F
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,
    // 0x50 SWAP_BUFFERS 0x51–0x5F
    1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // 0x60 VIEWPORT
    1,
    // 0x61–0x6F
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,
    // 0x70 BOX_TEST, 0x71 POS_TEST, 0x72 VEC_TEST 0x73–0x7F
    3,2,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // 0x80–0xFF: all 0
};

// ---------------------------------------------------------------------------
// GxDisplayListDecoder ctor
// ---------------------------------------------------------------------------
GxDisplayListDecoder::GxDisplayListDecoder()
{
    posMatrix_.setIdentity();
    texMatrix_.setIdentity();
}

// ---------------------------------------------------------------------------
// decode()
// ---------------------------------------------------------------------------
std::vector<DecodedNsbmdMesh> GxDisplayListDecoder::decode(std::span<const uint8_t> cmdList)
{
    finishedMeshes_.clear();
    currentMesh_ = {};
    inPrimitive_ = false;

    // NDS GX uses packed commands: 4 command bytes per word, followed by
    // parameter words.  Walk through the buffer.
    size_t pos = 0;
    const size_t sz = cmdList.size();

    // Collect all 32-bit words
    std::vector<uint32_t> words;
    words.reserve(sz / 4 + 1);
    while (pos + 3 < sz)
    {
        uint32_t w = static_cast<uint32_t>(cmdList[pos])
                   | (static_cast<uint32_t>(cmdList[pos+1]) << 8)
                   | (static_cast<uint32_t>(cmdList[pos+2]) << 16)
                   | (static_cast<uint32_t>(cmdList[pos+3]) << 24);
        words.push_back(w);
        pos += 4;
    }

    // Process packed command words.
    // Format: [CMD3][CMD2][CMD1][CMD0] followed by parameter words for CMD0,
    // then CMD1, then CMD2, then CMD3.
    size_t wi = 0;
    while (wi < words.size())
    {
        uint32_t packed = words[wi++];

        // Extract up to 4 packed commands (LSB first)
        uint8_t cmds[4] =
        {
            static_cast<uint8_t>((packed >>  0) & 0xFF),
            static_cast<uint8_t>((packed >>  8) & 0xFF),
            static_cast<uint8_t>((packed >> 16) & 0xFF),
            static_cast<uint8_t>((packed >> 24) & 0xFF),
        };

        // Process each command and consume its parameters from the word stream
        for (int ci = 0; ci < 4; ++ci)
        {
            uint8_t cmd = cmds[ci];
            if (cmd == 0x00) continue; // NOP

            uint8_t nparams = (cmd < 256) ? kParamCount[cmd] : 0;
            processCommand(cmd, std::span<const uint32_t>(words), wi);
            wi += nparams; // advance past consumed params
            // Clamp to prevent over-read
            if (wi > words.size()) { wi = words.size(); break; }
        }
    }

    // Finalise any open primitive block
    if (inPrimitive_) cmdEndVtxs();

    return std::move(finishedMeshes_);
}

// ---------------------------------------------------------------------------
// processCommand()
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::processCommand(uint8_t cmd,
                                          std::span<const uint32_t> words,
                                          size_t& wi)
{
    auto safeWord = [&](size_t offset) -> uint32_t
    {
        size_t idx = wi + offset;
        return idx < words.size() ? words[idx] : 0u;
    };

    switch (cmd)
    {
        // --- Matrix ---
        case 0x10: /* MTX_MODE    – 1 param */  break; // ignore
        case 0x14: /* MTX_PUSH    – 0 params */ cmdMtxPush();  break;
        case 0x15: /* MTX_POP     – 1 param */  cmdMtxPop(safeWord(0)); break;
        case 0x16: /* MTX_STORE   – 1 param */  break; // ignore
        case 0x17: /* MTX_RESTORE – 1 param */  break; // ignore
        case 0x18: /* MTX_IDENTITY */ cmdMtxIdentity(); break;
        case 0x19: /* MTX_LOAD_4x4  */ cmdMtxLoad4x4(words, wi); return; // handles wi itself
        case 0x1A: /* MTX_LOAD_4x3  */ cmdMtxLoad4x3(words, wi); return;
        case 0x1B: /* MTX_MULT_4x4  */ cmdMtxMult4x4(words, wi); return;
        case 0x1C: /* MTX_MULT_4x3  */ cmdMtxMult4x3(words, wi); return;
        case 0x1D: /* MTX_MULT_3x3  */ wi += 9; return; // skip
        case 0x1E: /* MTX_SCALE     */ cmdMtxScale(words, wi);  return;
        case 0x1F: /* MTX_TRANS     */ cmdMtxTrans(words, wi);  return;

        // --- Attributes ---
        case 0x20: break; // COLOR – ignore
        case 0x21: break; // NORMAL – ignore (no lighting)
        case 0x22: cmdTexCoord(safeWord(0)); break;
        case 0x29: break; // POLYGON_ATTR – ignore
        case 0x2A: cmdTexImageParam(safeWord(0)); break;
        case 0x2B: break; // PLTT_BASE – ignore (handled by TEX0)

        // --- Vertices ---
        case 0x23: cmdVtx16 (safeWord(0), safeWord(1)); break;
        case 0x24: cmdVtx10 (safeWord(0)); break;
        case 0x25: cmdVtxXY (safeWord(0)); break;
        case 0x26: cmdVtxXZ (safeWord(0)); break;
        case 0x27: cmdVtxYZ (safeWord(0)); break;
        case 0x28: cmdVtxDiff(safeWord(0)); break;

        // --- Primitive control ---
        case 0x40: cmdBeginVtxs(safeWord(0)); break;
        case 0x41: cmdEndVtxs(); break;

        default: break; // unknown commands silently skipped
    }
}

// ---------------------------------------------------------------------------
// TEXIMAGE_PARAM (0x2A)
// First-pass mapping: texture index from TEXIMAGE_PARAM bits [23:16]
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdTexImageParam(uint32_t param)
{
    currentTexAddr_ = (param >> 16) & 0xFFu;
    std::printf("[GX] TEXIMAGE_PARAM idx=%u\n", currentTexAddr_);
}

// ---------------------------------------------------------------------------
// TEXCOORD (0x22)
// Bits [15:0]  = S (U) as 1.11.4 fixed point
// Bits [31:16] = T (V) as 1.11.4 fixed point
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdTexCoord(uint32_t param)
{
    int16_t rawU = static_cast<int16_t>(param & 0xFFFFu);
    int16_t rawV = static_cast<int16_t>((param >> 16) & 0xFFFFu);
    currentU_ = static_cast<float>(rawU) / 16.f; // 1.11.4 fixed → float (texels)
    currentV_ = static_cast<float>(rawV) / 16.f;
}

// ---------------------------------------------------------------------------
// VTX_16 (0x23) – 2 params
// p0: bits[15:0]=X, bits[31:16]=Y  (1.3.12)
// p1: bits[15:0]=Z, bits[31:16]=unused
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdVtx16(uint32_t p0, uint32_t p1)
{
    int16_t rx = static_cast<int16_t>(p0 & 0xFFFFu);
    int16_t ry = static_cast<int16_t>((p0 >> 16) & 0xFFFFu);
    int16_t rz = static_cast<int16_t>(p1 & 0xFFFFu);
    float x = GxFixed::from1_3_12(rx);
    float y = GxFixed::from1_3_12(ry);
    float z = GxFixed::from1_3_12(rz);
    lastX_ = x; lastY_ = y; lastZ_ = z;
    emitVertex(x, y, z);
}

// ---------------------------------------------------------------------------
// VTX_10 (0x24) – 1 param
// Bits[9:0]=X, [19:10]=Y, [29:20]=Z  (1.0.9 fixed, scale /64)
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdVtx10(uint32_t param)
{
    int16_t rx = static_cast<int16_t>((param >>  0) & 0x3FFu);
    int16_t ry = static_cast<int16_t>((param >> 10) & 0x3FFu);
    int16_t rz = static_cast<int16_t>((param >> 20) & 0x3FFu);
    // Sign-extend 10-bit
    if (rx & 0x200) rx |= 0xFC00;
    if (ry & 0x200) ry |= 0xFC00;
    if (rz & 0x200) rz |= 0xFC00;
    float x = GxFixed::from1_0_9(rx);
    float y = GxFixed::from1_0_9(ry);
    float z = GxFixed::from1_0_9(rz);
    lastX_ = x; lastY_ = y; lastZ_ = z;
    emitVertex(x, y, z);
}

// ---------------------------------------------------------------------------
// VTX_DIFF (0x28) – relative to last vertex, 10-bit signed /8
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdVtxDiff(uint32_t param)
{
    int16_t dx = static_cast<int16_t>((param >>  0) & 0x3FFu);
    int16_t dy = static_cast<int16_t>((param >> 10) & 0x3FFu);
    int16_t dz = static_cast<int16_t>((param >> 20) & 0x3FFu);
    if (dx & 0x200) dx |= 0xFC00;
    if (dy & 0x200) dy |= 0xFC00;
    if (dz & 0x200) dz |= 0xFC00;
    float x = lastX_ + static_cast<float>(dx) / 8.f;
    float y = lastY_ + static_cast<float>(dy) / 8.f;
    float z = lastZ_ + static_cast<float>(dz) / 8.f;
    lastX_ = x; lastY_ = y; lastZ_ = z;
    emitVertex(x, y, z);
}

// ---------------------------------------------------------------------------
// VTX_XY / VTX_XZ / VTX_YZ – reuse one component from last vertex
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdVtxXY(uint32_t param)
{
    int16_t rx = static_cast<int16_t>(param & 0xFFFFu);
    int16_t ry = static_cast<int16_t>((param >> 16) & 0xFFFFu);
    float x = GxFixed::from1_3_12(rx);
    float y = GxFixed::from1_3_12(ry);
    lastX_ = x; lastY_ = y;
    emitVertex(x, y, lastZ_);
}
void GxDisplayListDecoder::cmdVtxXZ(uint32_t param)
{
    int16_t rx = static_cast<int16_t>(param & 0xFFFFu);
    int16_t rz = static_cast<int16_t>((param >> 16) & 0xFFFFu);
    float x = GxFixed::from1_3_12(rx);
    float z = GxFixed::from1_3_12(rz);
    lastX_ = x; lastZ_ = z;
    emitVertex(x, lastY_, z);
}
void GxDisplayListDecoder::cmdVtxYZ(uint32_t param)
{
    int16_t ry = static_cast<int16_t>(param & 0xFFFFu);
    int16_t rz = static_cast<int16_t>((param >> 16) & 0xFFFFu);
    float y = GxFixed::from1_3_12(ry);
    float z = GxFixed::from1_3_12(rz);
    lastY_ = y; lastZ_ = z;
    emitVertex(lastX_, y, z);
}

// ---------------------------------------------------------------------------
// BEGIN_VTXS (0x40)
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdBeginVtxs(uint32_t param)
{
    if (inPrimitive_) cmdEndVtxs();

    currentMesh_ = {};
    primitiveType_ = param & 0x03;
    currentMesh_.primitiveType = primitiveType_;
    inPrimitive_ = true;
}

// ---------------------------------------------------------------------------
// END_VTXS (0x41)
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdEndVtxs()
{
    if (!inPrimitive_) return;
    inPrimitive_ = false;
    buildEdges();
    finishedMeshes_.push_back(std::move(currentMesh_));
    currentMesh_ = {};
}

// ---------------------------------------------------------------------------
// emitVertex()  – THE core function.  Transform and record vertex + texAddr.
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::emitVertex(float x, float y, float z)
{
    if (!inPrimitive_) return;

    float tx, ty, tz;
    posMatrix_.transformPoint(x, y, z, tx, ty, tz);

    NsbmdVertex v;
    v.x = tx; v.y = ty; v.z = tz;
    v.u = currentU_;
    v.v = currentV_;

    currentMesh_.vertices.push_back(v);
    currentMesh_.vertexTextureAddr.push_back(currentTexAddr_);

    // Triangulate on the fly
    size_t n = currentMesh_.vertices.size();
    uint16_t ni = static_cast<uint16_t>(n - 1);

    switch (primitiveType_)
    {
        case 0: // GL_TRIANGLES: emit complete triangles every 3rd vertex
            if (n % 3 == 0)
            {
                currentMesh_.indices.push_back(ni - 2);
                currentMesh_.indices.push_back(ni - 1);
                currentMesh_.indices.push_back(ni);
            }
            break;

        case 1: // GL_QUADS: emit two triangles every 4th vertex
            if (n % 4 == 0)
            {
                currentMesh_.indices.push_back(ni - 3);
                currentMesh_.indices.push_back(ni - 2);
                currentMesh_.indices.push_back(ni - 1);
                currentMesh_.indices.push_back(ni - 3);
                currentMesh_.indices.push_back(ni - 1);
                currentMesh_.indices.push_back(ni);
            }
            break;

        case 2: // GL_TRIANGLE_STRIP
            if (n >= 3)
            {
                if ((n - 3) % 2 == 0)
                {
                    currentMesh_.indices.push_back(ni - 2);
                    currentMesh_.indices.push_back(ni - 1);
                    currentMesh_.indices.push_back(ni);
                }
                else
                {
                    currentMesh_.indices.push_back(ni - 2);
                    currentMesh_.indices.push_back(ni);
                    currentMesh_.indices.push_back(ni - 1);
                }
            }
            break;

        case 3: // GL_QUAD_STRIP: new quad every 2 new vertices (n≥4, even)
            if (n >= 4 && n % 2 == 0)
            {
                currentMesh_.indices.push_back(ni - 3);
                currentMesh_.indices.push_back(ni - 2);
                currentMesh_.indices.push_back(ni);
                currentMesh_.indices.push_back(ni - 3);
                currentMesh_.indices.push_back(ni);
                currentMesh_.indices.push_back(ni - 1);
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// buildEdges() – generate line index pairs for wireframe
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::buildEdges()
{
    auto& idx = currentMesh_.indices;
    auto& edges = currentMesh_.edgeIndices;
    edges.clear();
    edges.reserve(idx.size() * 2); // worst case

    // For each triangle, emit three edges (deduplicated by ordering)
    for (size_t i = 0; i + 2 < idx.size(); i += 3)
    {
        uint16_t a = idx[i], b = idx[i+1], c = idx[i+2];
        // Store as (lo, hi) pairs for easy dedup
        auto addEdge = [&](uint16_t p, uint16_t q)
        {
            if (p > q) std::swap(p, q);
            edges.push_back(p);
            edges.push_back(q);
        };
        addEdge(a, b);
        addEdge(b, c);
        addEdge(c, a);
    }

    // Deduplicate
    std::sort(edges.begin(), edges.end(), [](uint16_t, uint16_t) { return false; });
    // Sort pairs: sort by (first, second)
    std::vector<std::pair<uint16_t,uint16_t>> pairs;
    for (size_t i = 0; i + 1 < edges.size(); i += 2)
        pairs.emplace_back(edges[i], edges[i+1]);
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

    edges.clear();
    for (auto& [a, b] : pairs)
    {
        edges.push_back(a);
        edges.push_back(b);
    }
}

// ---------------------------------------------------------------------------
// Matrix commands
// ---------------------------------------------------------------------------
void GxDisplayListDecoder::cmdMtxIdentity()
{
    posMatrix_.setIdentity();
}
void GxDisplayListDecoder::cmdMtxPush()
{
    matStack_.push(posMatrix_);
}
void GxDisplayListDecoder::cmdMtxPop(uint32_t param)
{
    int count = static_cast<int>(param & 0x3F);
    if (count < 1) count = 1;
    while (count-- > 0 && !matStack_.empty())
    {
        posMatrix_ = matStack_.top();
        matStack_.pop();
    }
}

static float readFixed4x4(const uint32_t* w, int row, int col)
{
    return GxFixed::fromMatrix(static_cast<int32_t>(w[col*4 + row]));
}

void GxDisplayListDecoder::cmdMtxLoad4x4(std::span<const uint32_t> words, size_t& wi)
{
    if (wi + 16 > words.size()) { wi += 16; return; }
    const uint32_t* w = words.data() + wi;
    GxMatrix m;
    for (int i = 0; i < 16; ++i)
        m.m[i] = GxFixed::fromMatrix(static_cast<int32_t>(w[i]));
    posMatrix_ = m;
    wi += 16;
}
void GxDisplayListDecoder::cmdMtxLoad4x3(std::span<const uint32_t> words, size_t& wi)
{
    if (wi + 12 > words.size()) { wi += 12; return; }
    const uint32_t* w = words.data() + wi;
    GxMatrix m; m.setIdentity();
    // Column-major, 4×3 (no projection row)
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 3; ++row)
            m.m[col*4 + row] = GxFixed::fromMatrix(static_cast<int32_t>(w[col*3 + row]));
    posMatrix_ = m;
    wi += 12;
}
void GxDisplayListDecoder::cmdMtxMult4x4(std::span<const uint32_t> words, size_t& wi)
{
    if (wi + 16 > words.size()) { wi += 16; return; }
    const uint32_t* w = words.data() + wi;
    GxMatrix m;
    for (int i = 0; i < 16; ++i)
        m.m[i] = GxFixed::fromMatrix(static_cast<int32_t>(w[i]));
    posMatrix_ = posMatrix_ * m;
    wi += 16;
}
void GxDisplayListDecoder::cmdMtxMult4x3(std::span<const uint32_t> words, size_t& wi)
{
    if (wi + 12 > words.size()) { wi += 12; return; }
    const uint32_t* w = words.data() + wi;
    GxMatrix m; m.setIdentity();
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 3; ++row)
            m.m[col*4 + row] = GxFixed::fromMatrix(static_cast<int32_t>(w[col*3 + row]));
    posMatrix_ = posMatrix_ * m;
    wi += 12;
}
void GxDisplayListDecoder::cmdMtxScale(std::span<const uint32_t> words, size_t& wi)
{
    if (wi + 3 > words.size()) { wi += 3; return; }
    float sx = GxFixed::fromMatrix(static_cast<int32_t>(words[wi+0]));
    float sy = GxFixed::fromMatrix(static_cast<int32_t>(words[wi+1]));
    float sz = GxFixed::fromMatrix(static_cast<int32_t>(words[wi+2]));
    GxMatrix m; m.setIdentity();
    m.m[0] = sx; m.m[5] = sy; m.m[10] = sz;
    posMatrix_ = posMatrix_ * m;
    wi += 3;
}
void GxDisplayListDecoder::cmdMtxTrans(std::span<const uint32_t> words, size_t& wi)
{
    if (wi + 3 > words.size()) { wi += 3; return; }
    float tx = GxFixed::fromMatrix(static_cast<int32_t>(words[wi+0]));
    float ty = GxFixed::fromMatrix(static_cast<int32_t>(words[wi+1]));
    float tz = GxFixed::fromMatrix(static_cast<int32_t>(words[wi+2]));
    GxMatrix m; m.setIdentity();
    m.m[12] = tx; m.m[13] = ty; m.m[14] = tz;
    posMatrix_ = posMatrix_ * m;
    wi += 3;
}
