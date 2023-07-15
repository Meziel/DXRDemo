#include "Common.hlsli"

struct ClearColor
{
    float4 Color;
};

ConstantBuffer<ClearColor> ClearColorCB : register(b0);

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.Li = payload.Depth > 0 ? 0 : ClearColorCB.Color.rgb;
    payload.Distance = -1.f;

}