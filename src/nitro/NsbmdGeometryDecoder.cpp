#include "NsbmdGeometryDecoder.hpp"

#include <QMatrix4x4>
#include <cstdio>

namespace nitro
{

enum
{
    GX_TRIANGLES      = 0,
    GX_QUADS          = 1,
    GX_TRIANGLE_STRIP = 2,
    GX_QUAD_STRIP     = 3
};

struct GxState
{
    QMatrix4x4 currentMtx;
    QVector3D currentPos {0.f, 0.f, 0.f};

    float currentU = 0.f;
    float currentV = 0.f;

    uint8_t primitiveMode = 0;
    bool primitiveActive = false;

    std::vector<uint32_t> primVerts;
    bool stripFlip = false;

    uint32_t currentTexAddr = 0;

    GxState() { currentMtx.setToIdentity(); }
};

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

static void addEdge(DecodedNsbmdMesh& mesh, uint32_t a, uint32_t b)
{
    mesh.edges.push_back({a, b});
}

static void emitTriangle(DecodedNsbmdMesh& mesh, uint32_t a, uint32_t b, uint32_t c)
{
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);

    addEdge(mesh, a, b);
    addEdge(mesh, b, c);
    addEdge(mesh, c, a);
}

static uint32_t pushVertex(DecodedNsbmdMesh& mesh,
                           const QVector3D& pos,
                           const QVector2D& uv,
                           uint32_t texAddr)
{
    mesh.vertices.push_back(pos);
    mesh.uvs.push_back(uv);
    mesh.vertexTextureAddr.push_back(texAddr);
    return static_cast<uint32_t>(mesh.vertices.size() - 1);
}

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
            const size_t n = s.primVerts.size();
            emitTriangle(mesh, s.primVerts[n - 3], s.primVerts[n - 2], s.primVerts[n - 1]);
        }
        break;

    case GX_QUADS:
        if (s.primVerts.size() >= 4)
        {
            const size_t n = s.primVerts.size();
            const uint32_t a = s.primVerts[n - 4];
            const uint32_t b = s.primVerts[n - 3];
            const uint32_t c = s.primVerts[n - 2];
            const uint32_t d = s.primVerts[n - 1];

            emitTriangle(mesh, a, b, c);
            emitTriangle(mesh, a, c, d);
        }
        break;

    case GX_TRIANGLE_STRIP:
        if (s.primVerts.size() >= 3)
        {
            const size_t n = s.primVerts.size();
            const uint32_t a = s.primVerts[n - 3];
            const uint32_t b = s.primVerts[n - 2];
            const uint32_t c = s.primVerts[n - 1];

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
            const size_t n = s.primVerts.size();
            const uint32_t a = s.primVerts[n - 4];
            const uint32_t b = s.primVerts[n - 3];
            const uint32_t c = s.primVerts[n - 2];
            const uint32_t d = s.primVerts[n - 1];

            emitTriangle(mesh, a, b, c);
            emitTriangle(mesh, b, d, c);
        }
        break;
    }
}

DecodedNsbmdMesh NsbmdGeometryDecoder::decodeWireframeMesh(const std::filesystem::path& path)
{
    DecodedNsbmdMesh mesh;

    std::vector<uint8_t> dl;

    (void)path;

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
            s.primitiveMode = dl[pc++];
            s.primitiveActive = true;
            s.primVerts.clear();
            s.stripFlip = false;
            break;

        case 0x41: // END
            s.primitiveActive = false;
            break;

        case 0x22: // TEXCOORD
        {
            uint32_t p = readU32(dl, pc);
            const int16_t rawU = static_cast<int16_t>(p & 0xFFFF);
            const int16_t rawV = static_cast<int16_t>((p >> 16) & 0xFFFF);
            s.currentU = static_cast<float>(rawU) / 16.f;
            s.currentV = static_cast<float>(rawV) / 16.f;
            break;
        }

        case 0x23: // VTX_16
        {
            int16_t x = static_cast<int16_t>(dl[pc] | (dl[pc + 1] << 8));
            int16_t y = static_cast<int16_t>(dl[pc + 2] | (dl[pc + 3] << 8));
            int16_t z = static_cast<int16_t>(dl[pc + 4] | (dl[pc + 5] << 8));
            pc += 6;

            QVector3D pos(x / 4096.f, y / 4096.f, z / 4096.f);
            QVector3D world = s.currentMtx * pos;

            const uint32_t idx = pushVertex(mesh, world, QVector2D(s.currentU, s.currentV), s.currentTexAddr);
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

            const uint32_t idx = pushVertex(mesh, world, QVector2D(s.currentU, s.currentV), s.currentTexAddr);
            processVertex(s, mesh, idx);
            break;
        }

        case 0x25: // VTX_DIFF
        {
            uint32_t p = readU32(dl, pc);

            int dx = sign10(p & 0x3FF);
            int dy = sign10((p >> 10) & 0x3FF);
            int dz = sign10((p >> 20) & 0x3FF);

            s.currentPos += QVector3D(dx / (4096.f * 8.f), dy / (4096.f * 8.f), dz / (4096.f * 8.f));

            QVector3D world = s.currentMtx * s.currentPos;

            const uint32_t idx = pushVertex(mesh, world, QVector2D(s.currentU, s.currentV), s.currentTexAddr);
            processVertex(s, mesh, idx);
            break;
        }

        case 0x2A: // TEXIMAGE_PARAM
        {
            uint32_t param = readU32(dl, pc);
            s.currentTexAddr = (param >> 16) & 0xFF;
            std::printf("[GX] TEXIMAGE_PARAM idx=%u\n", s.currentTexAddr);
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
