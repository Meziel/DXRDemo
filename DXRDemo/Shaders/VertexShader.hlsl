#include "Common.hlsli"

struct ModelViewProjection
{
    matrix MVP;
};

struct VertexShaderOutput
{
    float4 Color : COLOR;
    float4 Position : SV_Position;
    float3 WorldPosition : WORLD_POSITION;
    float3 Normal : NORMAL;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

VertexShaderOutput main(VertexData IN)
{
    VertexShaderOutput OUT;
 
    OUT.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.0f));
    OUT.WorldPosition = IN.Position;
    OUT.Color = IN.Color;
    OUT.Normal = IN.Normal;
 
    return OUT;
}