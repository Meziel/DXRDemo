#include "Common.hlsl"

StructuredBuffer<VertexData> vertices : register(t0);
StructuredBuffer<int> indices : register(t1);
RaytracingAccelerationStructure SceneBVH : register(t2);

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    // TODO: move to constant
    float3 lightPos = float3(0, 52, 0);
    float lightRadius = 200;
    
    float3 worldHit = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDir = normalize(lightPos - worldHit);
    float lightDistance = length(lightPos - worldHit);
    
    float attenuation = saturate(1.0f - (lightDistance / lightRadius));
    
    // Fire a shadow ray. The direction is hard-coded here, but can be fetched
    // from a constant-buffer
    RayDesc ray;
    ray.Origin = worldHit;
    ray.Direction = lightDir;
    ray.TMin = 0.01;
    ray.TMax = lightDistance - 0.001;
    
    ShadowHitInfo shadowPayload;
    
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
    float3 hitColor = vertices[indices[vertId + 0]].Color.rgb * barycentrics.x +
                      vertices[indices[vertId + 1]].Color.rgb * barycentrics.y +
                      vertices[indices[vertId + 2]].Color.rgb * barycentrics.z;
    payload.colorAndDistance = float4(hitColor * shadowColorFactor * attenuation, RayTCurrent());
}
