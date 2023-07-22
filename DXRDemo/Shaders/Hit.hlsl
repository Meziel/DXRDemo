#include "Common.hlsli"
#include "RandomNumberGenerator.hlsli"

StructuredBuffer<VertexData> vertices : register(t0);
StructuredBuffer<int> indices : register(t1);
ConstantBuffer<Settings> settings : register(b0);
RaytracingAccelerationStructure SceneBVH : register(t2);

// Quaternion-Quaternion Multiplication
float4 QMul(float4 q1, float4 q2)
{
    float3 v1 = q1.xyz, v2 = q2.xyz;
    float s1 = q1.w, s2 = q2.w;
    float3 v = s1 * v2 + s2 * v1 + cross(v1, v2);
    float s = s1 * s2 - dot(v1, v2);
    return float4(v, s);
}

// Quaternion-Vector Multiplication
float3 QMulVector(float4 q, float3 v)
{
    float4 v4 = float4(v, 0);
    float4 iq = float4(-q.xyz, q.w);
    float4 u = QMul(QMul(q, v4), iq);
    return u.xyz;
}


float3 RandomUnitVector(float2 randomSample)
{
    float z = randomSample.x * 2 - 1;
    float r = sqrt(max(1 - z * z, 0));
    float phi = 2 * PI * randomSample.y;
    return float3(r * cos(phi), r * sin(phi), z);
}

float3 ChangeDirectionReference(float3 direction, float3 oldRef, float3 newRef)
{
    // Compute the rotation quaternion between ref and d2
    float d = dot(oldRef, newRef);
    float4 quaternion;
    if (d > 0.99999)
    {
        return direction;
    }
    else if (d < -0.99999)
    {
        return -direction;
    }

    float3 c = cross(oldRef, newRef);
    float s = sqrt((1 + d) * 2);
    float invs = 1 / s;
    quaternion = normalize(float4(c * invs, s * 0.5));

    // Rotate d1 using quaternion
    return QMulVector(quaternion, direction);
}

float3 RandomUnitVectorHemisphere(float2 randomSample)
{
    float z = randomSample.x;
    float r = sqrt(max(1 - z * z, 0));
    float phi = 2 * PI * randomSample.y;
    return float3(r * cos(phi), r * sin(phi), z);
}

float ComputeBSDF(float3 v1, float3 v2, float3 normal)
{
    if (dot(normal, v2) >= 0)
    {
        return 1 / (2 * PI);
    }
    return 0;
}

float TranslateDomainToRange(float x, float d1, float d2, float r1, float r2)
{
    return r1 + ((x - d1) / (d2 - d1)) * (r2 - r1);
}

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    // TODO: move to constant
    float3 lightPos = float3(0, 52.4924, 0);
    float3 lightRadius = 20;
    
    float3 worldHit = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDirection = normalize(lightPos - worldHit);
    float lightDistance = length(lightPos - worldHit);
    
    float3 barycentrics = float3(1 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint vertId = 3 * PrimitiveIndex();
    
    VertexData vertexHitData[3] =
    {
        vertices[indices[vertId + 0]],
            vertices[indices[vertId + 1]],
            vertices[indices[vertId + 2]]
    };
    
    float3 hitColor = vertexHitData[0].Color.rgb * barycentrics.x +
                          vertexHitData[1].Color.rgb * barycentrics.y +
                          vertexHitData[2].Color.rgb * barycentrics.z;
    
    float3 hitNormal = vertexHitData[0].Normal.rgb * barycentrics.x +
                           vertexHitData[1].Normal.rgb * barycentrics.y +
                           vertexHitData[2].Normal.rgb * barycentrics.z;
    
    float3 hitEmissive = vertexHitData[0].Emission.rgb * barycentrics.x +
                           vertexHitData[1].Emission.rgb * barycentrics.y +
                           vertexHitData[2].Emission.rgb * barycentrics.z;
    
    payload.Li = hitEmissive * settings.lightIntensity;
    
    if (length(payload.Li) > 0)
    {
        return;
    }
    
    if (payload.Depth >= settings.bounces)
    {
        return;
    }
        
    uint seed = ((((payload.Depth * settings.bounces)
        + payload.Sample) * settings.samples
        + DispatchRaysIndex().x) * DispatchRaysDimensions().x
        + DispatchRaysIndex().y) * DispatchRaysDimensions().y;
        
    float random = RNG::Random01(seed);
    seed += 1;
        
    // Random sample
    float2 randomSample;
    randomSample.x = RNG::Random01(seed);
    seed += 1;
    randomSample.y = RNG::Random01(seed);
    
    float px = 1 / (2 * PI);
    if (settings.importanceSamplingEnabled)
    {
        float probToLight = settings.importanceSamplingPercentage;
        //float lightZ = 0.9;
        float lightZ = cos(atan(lightRadius / lightDistance));
        if (random > (1 - probToLight))
        {
            // Choose direction towards light
            randomSample.x = randomSample.x = TranslateDomainToRange(randomSample.x, -1, 1, lightZ, 1);
            float percOfSurfaceArea = (1 - lightZ) / 2;
            float surfaceArea = 4 * PI * percOfSurfaceArea;
            px = probToLight * (1 / surfaceArea);
        }
        else
        {
            // Choose direction away from light
            randomSample.x = TranslateDomainToRange(randomSample.x, -1, 1, -1, lightZ);
            float percOfSurfaceArea = (lightZ + 1) / 2;
            float surfaceArea = 4 * PI * percOfSurfaceArea;
            px = (1 - probToLight) * (1 / surfaceArea);
        }
    }
        
    // Generate direction
    float3 randomRayDirection = RandomUnitVector(randomSample);
    randomRayDirection = ChangeDirectionReference(randomRayDirection, float3(0, 0, 1), lightDirection);
    
    float bsdf = ComputeBSDF(-WorldRayDirection(), randomRayDirection, hitNormal);
        
    if (bsdf > 0)
    {
        HitInfo liPayload;
        liPayload.Depth = payload.Depth + 1;
        liPayload.Sample = payload.Sample;
        
        RayDesc ray;
        ray.Origin = worldHit;
        ray.TMin = 0.01;
        ray.TMax = 100000;
        ray.Direction = randomRayDirection;
        
        TraceRay(
        SceneBVH, // Acceleration Structure
        RAY_FLAG_NONE,
        0xFF,
        0, // Normal ray type
        2, // Hit group stride
        0, // Miss normal ray type 
        ray,
        liPayload);
            
        float n_dot_r = max(dot(ray.Direction, hitNormal), 0);
            
        float3 irradience = liPayload.Li * abs(n_dot_r);
            
        payload.Li += bsdf * hitColor * irradience / px;
    }
}
