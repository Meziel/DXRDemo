#include "Common.hlsli"
#include "RandomNumberGenerator.hlsli"

StructuredBuffer<VertexData> vertices : register(t0);
StructuredBuffer<int> indices : register(t1);
RaytracingAccelerationStructure SceneBVH : register(t2);

float3 RandomUnitVector(float2 random)
{
    // Generate the azimuthal angle (phi), 2pi radians around
    float phi = 2.0 * PI * random.x;

    // Generate the polar angle (theta)
    float theta = acos(2.0 * random.y - 1.0);

    // Convert from spherical to Cartesian coordinates
    float3 unitVector;
    unitVector.x = sin(theta) * cos(phi);
    unitVector.y = sin(theta) * sin(phi);
    unitVector.z = cos(theta);

    return unitVector;
}

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

float3 RandomUnitVectorHemisphere(float2 random, float3 normal)
{
    float z = random.x;
    float r = sqrt(max(1 - z * z, 0));
    float phi = 2 * PI * random.y;
    float3 unitVector = float3(r * cos(phi), r * sin(phi), z);
    
    float3 ref = float3(0.0, 0.0, 1.0);
    float3 newRef = normal; // Define your d2, make sure it's normalized
    
    // Compute the rotation quaternion between ref and d2
    float d = dot(ref, newRef);
    float4 quaternion;
    if (d > 0.99999)
    {
        return unitVector;
    }
    else if (d < -0.99999)
    {
        return -unitVector;
    }

    float3 c = cross(ref, newRef);
    float s = sqrt((1 + d) * 2);
    float invs = 1 / s;
    quaternion = normalize(float4(c * invs, s * 0.5));

    // Rotate d1 using quaternion
    return QMulVector(quaternion, unitVector);
}

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    // TODO: move to constant
    float3 lightPos = float3(0, 52.4924, 0);
    float lightIntensity = 50;
    
    float3 worldHit = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 lightDirection = normalize(lightPos - worldHit);
    float lightDistance = length(lightPos - worldHit);
    
    float3 barycentrics = float3(1 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint vertId = 3 * PrimitiveIndex();
    
    float bsdf = 1 / (2 * PI);
    
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
    
    payload.Li = hitEmissive * lightIntensity;
    
    //float attenuation = saturate(1.0f - (lightDistance / lightRadius));
    
    // Fire a shadow ray. The direction is hard-coded here, but can be fetched
    // from a constant-buffer 
    //{
    //    RayDesc ray;
    //    ray.Origin = worldHit;
    //    ray.Direction = lightDirection;
    //    ray.TMin = 0.01;
    //    ray.TMax = lightDistance - 0.001;
 

    //    ShadowHitInfo shadowPayload;
    
    //    // Trace the ray
    //    TraceRay(
    //      SceneBVH,
    //      RAY_FLAG_NONE,
    //      0xFF,
    //      1, // Shadow ray type
    //      2, // Hit group stride
    //      1, // Miss shadow ray type
    //      ray,
    //      shadowPayload);
    
    //    float shadowColorFactor = shadowPayload.isHit ? 0.2 : 1.0;
    
    //    float n_dot_l = max(dot(lightDirection, hitNormal), 0);
    
    //    payload.Li += float3(
    //        bsdf * hitColor * shadowColorFactor * n_dot_l * 4 * PI);
    //}
    
    {
        if (payload.Depth >= MAX_DEPTH)
        {
            return;
        }
        
        uint seed = ((((payload.Depth * MAX_DEPTH)
            + payload.Sample) * MAX_SAMPLES
            + DispatchRaysIndex().x) * DispatchRaysDimensions().x
            + DispatchRaysIndex().y) * DispatchRaysDimensions().y;
        
        
        float randomX = RNG::Random01(seed);
        seed += 1;
        float randomY = RNG::Random01(seed);
        
        // TODO: importance sampling
        //float stddev = 1 / 360;
        //float mean = 0;
        
        //// Use Box-Muller transform to generate Gaussian distributed numbers
        //float2 norm = sqrt(-2.0 * log(randomX)) * float2(cos(2.0 * PI * randomY), sin(2.0 * PI * randomY));

        //// Apply mean and standard deviation
        //norm *= stddev;
        //norm += mean;
        
        //float3 randomRayOffset = float3(randomX, randomY, randomZ);
        //float3 randomRayDirection = lightDirection + randomRayOffset;
        
        float3 randomRayDirection = RandomUnitVectorHemisphere(float2(randomX, randomY), hitNormal);
        
        static const int NUM_RAY_DIRECTION = 1;
        float3 rayDirections[] =
        {
            randomRayDirection
            //reflect(WorldRayDirection(), hitNormal),
            //lightDirection,
        };
    
        HitInfo liPayload;
        liPayload.Depth = payload.Depth + 1;
        liPayload.Sample = payload.Sample;
        
        RayDesc ray;
        ray.Origin = worldHit;
        ray.TMin = 0.01;
        ray.TMax = 100000;
        
        [unroll]
        for (int i = 0; i < NUM_RAY_DIRECTION; ++i)
        {
            ray.Direction = rayDirections[i];
        
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
            
            payload.Li += bsdf * hitColor * irradience * (2 * PI);
        }
    }
}
