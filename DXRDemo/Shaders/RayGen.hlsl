#include "Common.hlsl"

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
    payload.colorAndDistance = float4(0, 0, 0, 0);
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    // 0.5 centers pixel from [0, width-1] to [0.5, width-0.5] 
    float2 pixelCoordNDC = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
    float aspectRatio = dims.x / dims.y;
    
    // Define a ray, consisting of origin, direction, and the min-max distance values
    RayDesc ray;
    ray.Origin = mul(InvView.invMatrix, float4(0, 0, 0, 1)); // Origin of camera in world space
    float4 target = mul(InvProjection.invMatrix, float4(pixelCoordNDC.x, -pixelCoordNDC.y, 1, 1));
    ray.Direction = mul(InvView.invMatrix, float4(target.xyz, 0));
    ray.TMin = 0;
    ray.TMax = 100000;
    
    // Trace the ray
    TraceRay( 
    // Parameter name: AccelerationStructure
    // Acceleration structure
    SceneBVH,
    // Parameter name: RayFlags
    // Flags can be used to specify the behavior upon hitting a surface
    RAY_FLAG_NONE,
    0xFF,
    0,
    1,
    0.0, 
    ray,
    payload);
    
    gOutput[launchIndex] = float4(payload.colorAndDistance.rgb, 1.f);
}
