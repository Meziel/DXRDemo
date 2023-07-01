#include "Common.hlsl"

struct STriVertex
{
    float3 vertex;
    float4 color;
};
StructuredBuffer<STriVertex> BTriVertex : register(t0);
StructuredBuffer<int> indices : register(t1);


[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    float3 barycentrics = float3(1 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint vertId = 3 * PrimitiveIndex();
    float3 hitColor = BTriVertex[indices[vertId + 0]].color * barycentrics.x +
                      BTriVertex[indices[vertId + 1]].color * barycentrics.y +
                      BTriVertex[indices[vertId + 2]].color * barycentrics.z;
    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}
