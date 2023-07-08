struct VertexShaderOutput
{
    float4 Color : COLOR;
    float4 Position : SV_Position;
    float3 WorldPosition : WORLD_POSITION;
    float3 Normal : NORMAL;
};
 
float4 main(VertexShaderOutput IN) : SV_Target
{
    // TODO: make this constant buffers
    float3 lightPosition = float3(0, 52, 0);
    float lightColor = float3(1, 1, 1);
    float lightRadius = 200;
    float ambientColor = (float3) 0.3;
    
    float3 lightDirection = normalize(lightPosition - IN.WorldPosition.xyz);
    float lightDistance = length(lightPosition - IN.WorldPosition.xyz);
    float attenuation = saturate(1.0f - (lightDistance / lightRadius));
    float3 ambient = IN.Color.rgb * ambientColor;
    float n_dot_l = dot(lightDirection, IN.Normal);
    float3 diffuse = (n_dot_l > 0 ? IN.Color.rgb * n_dot_l * lightColor : (float3) 0);
    
    return float4(saturate(ambient + diffuse) * attenuation, IN.Color.a);
}

