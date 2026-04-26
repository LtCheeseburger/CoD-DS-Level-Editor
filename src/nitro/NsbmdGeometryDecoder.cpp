#include "NsbmdGeometryDecoder.hpp"

#include <QMatrix4x4>
#include <cstdio>

namespace nitro
{

// ------------------------------------------------------------
// Primitive Modes
// ------------------------------------------------------------
enum
{
    GX_TRIANGLES      = 0,
    GX_QUADS          = 1,
    GX_TRIANGLE_STRIP = 2,
    GX_QUAD_STRIP     = 3
};

// ------------------------------------------------------------
// GX STATE
// ------------------------------------------------------------
struct GxState
{
    QMatrix4x4 currentMtx;
    QVector3D currentPos {0.f, 0.f, 0.f};

    uint8_t primitiveMode = 0;
    bool primitiveActive = false;

    std::vector<uint32_t> primVerts;

    // strip helpers
    bool stripFlip = false;

    // texture state
    uint32_t currentTexAddr = 0;

    GxState()
    {
        currentMtx.setToIdentity();
    }
};

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static inline uint32_t readU32(const std::vector<uint8_t>& d, size_t& pc)
{
    uint32_t v =
        d[pc] |
        (d[pc + 1] << 8) |
        (d[pc + 2] << 16) |
        (d[pc + 3] << 24);
    pc += 4;
    return v;
}

static inline int sign10(int v)
{
    if (v & 0x200) v |= ~0x3FF;
    return v;
}

// ------------------------------------------------------------
// Edge Builder
// ------------------------------------------------------------
static void addEdge(DecodedNsbmdMesh& mesh, uint32_t a, uint32_t b)
{
    mesh.edges.push_back({a, b});
}

static void emitTriangle(DecodedNsbmdMesh& mesh, uint32_t a, uint32_t b, uint32_t c)
{
    addEdge(mesh, a, b);
    addEdge(mesh, b, c);
    addEdge(mesh, c, a);
}

// ------------------------------------------------------------
// Push Vertex
// ------------------------------------------------------------
static uint32_t pushVertex(DecodedNsbmdMesh& mesh, const QVector3D& pos, uint32_t texAddr)
{
    mesh.vertices.push_back(pos);
    mesh.vertexTextureAddr.push_back(texAddr);
    return static_cast<uint32_t>(mesh.vertices.size() - 1);
}

// ------------------------------------------------------------
// Primitive Assembly
// ------------------------------------------------------------
static void processVertex(GxState& s, DecodedNsbmdMesh& mesh, uint32_t idx)
{
    if (!s.primitiveActive)
        return;

    s.primVerts.push_back(idx);

    switch (s.primitiveMode)
    {
    case GX_TRIANGLES:
        if (s.primVerts.size() >= 3)
        {
            size_t n = s.primVerts.size();
            emitTriangle(mesh,
                s.primVerts[n - 3],
                s.primVerts[n - 2],
                s.primVerts[n - 1]);
        }
        break;

    case GX_QUADS:
        if (s.primVerts.size() >= 4)
        {
            size_t n = s.primVerts.size();

            uint32_t a = s.primVerts[n - 4];
            uint32_t b = s.primVerts[n - 3];
            uint32_t c = s.primVerts[n - 2];
            uint32_t d = s.primVerts[n - 1];

            emitTriangle(mesh, a, b, c);
            emitTriangle(mesh, a, c, d);
        }
        break;

    case GX_TRIANGLE_STRIP:
        if (s.primVerts.size() >= 3)
        {
            size_t n = s.primVerts.size();

            uint32_t a = s.primVerts[n - 3];
            uint32_t b = s.primVerts[n - 2];
            uint32_t c = s.primVerts[n - 1];

            if (s.stripFlip)
                emitTriangle(mesh, b, a, c);
            else
                emitTriangle(mesh, a, b, c);

            s.stripFlip = !s.stripFlip;
        }
        break;

    case GX_QUAD_STRIP:
        if (s.primVerts.size() >= 4)
        {
            size_t n = s.primVerts.size();

            uint32_t a = s.primVerts[n - 4];
            uint32_t b = s.primVerts[n - 3];
            uint32_t c = s.primVerts[n - 2];
            uint32_t d = s.primVerts[n - 1];

            emitTriangle(mesh, a, b, c);
            emitTriangle(mesh, b, d, c);
        }
        break;
    }
}

// ------------------------------------------------------------
// MAIN DECODER
// ------------------------------------------------------------
DecodedNsbmdMesh NsbmdGeometryDecoder::decodeWireframeMesh(const std::filesystem::path& path)
{
    DecodedNsbmdMesh mesh;

    std::vector<uint8_t> dl;

    // 🔴 KEEP YOUR EXISTING FILE LOADING HERE
    // (do NOT remove your NSBMD parsing logic)

    if (dl.empty())
        return mesh;

    GxState s;

    size_t pc = 0;
    const size_t end = dl.size();

    while (pc < end)
    {
        uint8_t cmd = dl[pc++];

        switch (cmd)
        {
        case 0x40: // BEGIN
        {
            s.primitiveMode = dl[pc++];
            s.primitiveActive = true;
            s.primVerts.clear();
            s.stripFlip = false;
            break;
        }

        case 0x41: // END
        {
            s.primitiveActive = false;
            break;
        }

        case 0x23: // VTX_16
        {
            int16_t x = (int16_t)(dl[pc] | (dl[pc + 1] << 8));
            int16_t y = (int16_t)(dl[pc + 2] | (dl[pc + 3] << 8));
            int16_t z = (int16_t)(dl[pc + 4] | (dl[pc + 5] << 8));
            pc += 6;

            QVector3D pos(x / 4096.f, y / 4096.f, z / 4096.f);
            QVector3D world = s.currentMtx * pos;

            uint32_t idx = pushVertex(mesh, world, s.currentTexAddr);
            processVertex(s, mesh, idx);
            break;
        }

        case 0x24: // VTX_10
        {
            uint32_t p = readU32(dl, pc);

            int x = sign10(p & 0x3FF);
            int y = sign10((p >> 10) & 0x3FF);
            int z = sign10((p >> 20) & 0x3FF);

            QVector3D pos(x / 4096.f, y / 4096.f, z / 4096.f);
            QVector3D world = s.currentMtx * pos;

            uint32_t idx = pushVertex(mesh, world, s.currentTexAddr);
            processVertex(s, mesh, idx);
            break;
        }

        case 0x25: // VTX_DIFF
        {
            uint32_t p = readU32(dl, pc);

            int dx = sign10(p & 0x3FF);
            int dy = sign10((p >> 10) & 0x3FF);
            int dz = sign10((p >> 20) & 0x3FF);

            s.currentPos += QVector3D(
                dx / (4096.f * 8.f),
                dy / (4096.f * 8.f),
                dz / (4096.f * 8.f));

            QVector3D world = s.currentMtx * s.currentPos;

            uint32_t idx = pushVertex(mesh, world, s.currentTexAddr);
            processVertex(s, mesh, idx);
            break;
        }

        case 0x2A: // TEXIMAGE_PARAM
        {
            uint32_t param = readU32(dl, pc);
            uint32_t texAddr = (param & 0xFFFF) << 3;
            s.currentTexAddr = texAddr;

            printf("[GX] TEXIMAGE_PARAM addr=0x%08X\n", texAddr);
            break;
        }

        default:
            break;
        }
    }

    mesh.valid = true;
    return mesh;
}

} // namespace nitro