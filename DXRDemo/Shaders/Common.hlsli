#ifndef COMMON_H
#define COMMON_H

static const float PI = 3.14159265f;
static const uint MAX_SAMPLES = 100;
static const uint MAX_DEPTH = 2;

struct VertexData
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float3 Emission : EMISSION;
};

struct HitInfo
{
  float3 Li;
  float Distance;
  uint Depth;
  uint Sample;
};

struct ShadowHitInfo
{
    bool isHit;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
  float2 bary;
};

#endif // COMMON_H