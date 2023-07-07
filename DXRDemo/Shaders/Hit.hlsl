#include "Common.hlsl"

struct STriVertex
{
    float3 vertex;
    float4 color;
};

StructuredBuffer<STriVertex> BTriVertex : register(t0);
StructuredBuffer<int> indices : register(t1);
RaytracingAccelerationStructure SceneBVH : register(t2);

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    float3 lightPos = float3(0, 500, 0);
    float3 worldHit = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDir = normalize(lightPos - worldHit);
    
    // Fire a shadow ray. The direction is hard-coded here, but can be fetched
    // from a constant-buffer
    RayDesc ray;
    ray.Origin = worldHit;
    ray.Direction = lightDir;
    ray.TMin = 0.01;
    ray.TMax = 100000;
    
    ShadowHitInfo shadowPayload;
    shadowPayload.isHit = false;
    
    // Trace the ray
    TraceRay(
      SceneBVH,
      RAY_FLAG_NONE,
      0xFF,
      1, // Shadow ray type
      2, // Hit group stride
      1, // Miss shadow ray type
      ray,
      shadowPayload);
    
    float shadowColorFactor = shadowPayload.isHit ? 0.3 : 1.0;
    
    // Default color
    float3 barycentrics = float3(1 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint vertId = 3 * PrimitiveIndex();
    float3 hitColor = BTriVertex[indices[vertId + 0]].color.rgb * barycentrics.x +
                      BTriVertex[indices[vertId + 1]].color.rgb * barycentrics.y +
                      BTriVertex[indices[vertId + 2]].color.rgb * barycentrics.z;
    payload.colorAndDistance = float4(hitColor * shadowColorFactor, RayTCurrent());
}
