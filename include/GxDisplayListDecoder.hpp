#pragma once
// =============================================================================
//  GxDisplayListDecoder.hpp
//  Decodes a Nintendo DS GX display list into DecodedNsbmdMesh objects.
//
//  Implements the GX command set relevant to NSBMD:
//      0x10  MTX_MODE
//      0x14  MTX_PUSH / 0x15 MTX_POP / 0x16 MTX_STORE / 0x17 MTX_RESTORE
//      0x18  MTX_IDENTITY
//      0x19  MTX_LOAD_4x4 / 0x1A MTX_LOAD_4x3
//      0x1B  MTX_MULT_4x4 / 0x1C MTX_MULT_4x3 / 0x1D MTX_MULT_3x3
//      0x1E  MTX_SCALE
//      0x1F  MTX_TRANS
//      0x20  COLOR
//      0x21  NORMAL
//      0x22  TEXCOORD
//      0x23  VTX_16
//      0x24  VTX_10
//      0x25  VTX_XY / 0x26 VTX_XZ / 0x27 VTX_YZ
//      0x28  VTX_DIFF
//      0x29  POLYGON_ATTR
//      0x2A  TEXIMAGE_PARAM   ← critical for texture binding
//      0x40  BEGIN_VTXS
//      0x41  END_VTXS
//      0x70  BOX_TEST (ignored)
// =============================================================================

#include "DecodedNsbmdMesh.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stack>
#include <vector>

// ---------------------------------------------------------------------------
// Fixed-point helpers
// ---------------------------------------------------------------------------
namespace GxFixed
{
    // 1.3.12 fixed point → float
    inline float from1_3_12(int16_t raw) { return static_cast<float>(raw) / 4096.f; }
    // 1.0.9 fixed (VTX_10) → float
    inline float from1_0_9 (int16_t raw)
    {
        // VTX_10: bits[9:0] as signed, scale by 1/64
        return static_cast<float>(raw) / 64.f;
    }
    // 1.11.4 fixed (matrix row) → float
    inline float fromMatrix(int32_t raw) { return static_cast<float>(raw) / 4096.f; }
}

// 4×4 column-major matrix
struct GxMatrix
{
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    void setIdentity()
    {
        m[0]=1; m[1]=0; m[2]=0; m[3]=0;
        m[4]=0; m[5]=1; m[6]=0; m[7]=0;
        m[8]=0; m[9]=0; m[10]=1; m[11]=0;
        m[12]=0; m[13]=0; m[14]=0; m[15]=1;
    }

    // Multiply this = this × rhs
    GxMatrix operator*(const GxMatrix& rhs) const
    {
        GxMatrix r; r.setIdentity();
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
            {
                float s = 0;
                for (int k = 0; k < 4; ++k)
                    s += m[row + k*4] * rhs.m[k + col*4];
                r.m[row + col*4] = s;
            }
        return r;
    }

    // Transform a vec3 (w=1)
    void transformPoint(float ix, float iy, float iz,
                        float& ox, float& oy, float& oz) const
    {
        ox = m[0]*ix + m[4]*iy + m[8] *iz + m[12];
        oy = m[1]*ix + m[5]*iy + m[9] *iz + m[13];
        oz = m[2]*ix + m[6]*iy + m[10]*iz + m[14];
    }
};

// ---------------------------------------------------------------------------
// GxDisplayListDecoder
// ---------------------------------------------------------------------------
class GxDisplayListDecoder
{
public:
    GxDisplayListDecoder();

    // -----------------------------------------------------------------------
    // decode()
    //   cmdList  – raw GX packed command bytes
    //   Returns one mesh per BEGIN_VTXS / END_VTXS block.
    // -----------------------------------------------------------------------
    std::vector<DecodedNsbmdMesh> decode(std::span<const uint8_t> cmdList);

private:
    // --- GX state ---
    GxMatrix                  posMatrix_;
    GxMatrix                  texMatrix_;
    std::stack<GxMatrix>      matStack_;

    // Texture state (persists across vertex emissions until TEXIMAGE_PARAM)
    uint32_t  currentTexAddr_  = 0;
    float     currentU_        = 0.f;
    float     currentV_        = 0.f;
    float     lastX_           = 0.f;
    float     lastY_           = 0.f;
    float     lastZ_           = 0.f;

    // Active primitive block
    bool      inPrimitive_     = false;
    uint8_t   primitiveType_   = 0;

    // Current mesh being built
    DecodedNsbmdMesh currentMesh_;

    // --- Command dispatch ---
    // Returns number of parameter words consumed (not counting the command byte itself)
    void processCommand(uint8_t cmd, std::span<const uint32_t> params, size_t& paramIdx);

    // Command handlers
    void cmdTexImageParam(uint32_t param);
    void cmdTexCoord     (uint32_t param);
    void cmdVtx16        (uint32_t p0, uint32_t p1);
    void cmdVtx10        (uint32_t param);
    void cmdVtxDiff      (uint32_t param);
    void cmdVtxXY        (uint32_t param);
    void cmdVtxXZ        (uint32_t param);
    void cmdVtxYZ        (uint32_t param);
    void cmdBeginVtxs    (uint32_t param);
    void cmdEndVtxs      ();
    void cmdMtxIdentity  ();
    void cmdMtxLoad4x4   (std::span<const uint32_t> params, size_t& idx);
    void cmdMtxLoad4x3   (std::span<const uint32_t> params, size_t& idx);
    void cmdMtxMult4x4   (std::span<const uint32_t> params, size_t& idx);
    void cmdMtxMult4x3   (std::span<const uint32_t> params, size_t& idx);
    void cmdMtxScale     (std::span<const uint32_t> params, size_t& idx);
    void cmdMtxTrans     (std::span<const uint32_t> params, size_t& idx);
    void cmdMtxPush      ();
    void cmdMtxPop       (uint32_t param);

    // Emit one transformed vertex to currentMesh_
    void emitVertex(float x, float y, float z);

    // Finalise edge list for currentMesh_ after END_VTXS
    void buildEdges();

    std::vector<DecodedNsbmdMesh> finishedMeshes_;
};
