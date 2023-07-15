#include "Common.hlsli"

struct InverseMatrix
{
    matrix invMatrix;
};

ConstantBuffer<InverseMatrix> InvProjection : register(b0);
ConstantBuffer<InverseMatrix> InvView : register(b1);

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

[shader("raygeneration")]
void RayGen()
{
    // Initialize the ray payload
    HitInfo payload;
    payload.Depth = 0;
    
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    // 0.5 centers pixel from [0, width-1] to [0.5, width-0.5] 
    float2 pixelCoordNDC = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
    float aspectRatio = dims.x / dims.y;
    
    // Define initial ray, consisting of origin, direction, and the min-max distance values
    RayDesc ray;
    ray.Origin = mul(InvView.invMatrix, float4(0, 0, 0, 1)); // Origin of camera in world space
    float4 target = mul(InvProjection.invMatrix, float4(pixelCoordNDC.x, -pixelCoordNDC.y, 1, 1));
    ray.Direction = mul(InvView.invMatrix, float4(target.xyz, 0));
    ray.TMin = 0.01;
    ray.TMax = 100000;
    
    int depth = 0;
    
    float3 Li = (float3) 0;
    
    for (uint i = 0; i < MAX_SAMPLES; ++i)
    {
        payload.Sample = i;
        
        TraceRay(
        SceneBVH, // Acceleration Structure
        RAY_FLAG_NONE,
        0xFF,
        0, // Normal ray type
        2, // Hit group stride
        0, // Miss normal ray type 
        ray,
        payload);
        Li += payload.Li;
    }
    Li /= MAX_SAMPLES;
    
    gOutput[launchIndex] = float4(Li, 1.f);
}
