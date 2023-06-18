#include "Common.hlsl"

struct ClearColor
{
    float4 color;
};

ConstantBuffer<ClearColor> ClearColorCB : register(b0);

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.colorAndDistance = float4(ClearColorCB.color.rgb, -1.f);
}