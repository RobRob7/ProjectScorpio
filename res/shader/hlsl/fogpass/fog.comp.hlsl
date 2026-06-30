#include "../helper.hlsl"

cbuffer UBO : register(b0)
{
    float4x4 u_invViewProj;

    float4 u_cameraPos;
    float4 u_sunColor;

    float u_maxDistance;
    float u_stepSize;
    float u_scatteringDensity;
    float u_absorptionDensity;
};

Texture2D<float> ForwardDepthTex : register(t1);
RWTexture2D<float4> FogImage : register(u2);

SamplerState LinearClampSampler : register(s0);

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pixel = int2(dispatchThreadID.xy);
    
    uint fogWidth;
    uint fogHeight;
    FogImage.GetDimensions(fogWidth, fogHeight);
    
    int2 fogSize = int2(fogWidth, fogHeight);

    if(pixel.x >= fogSize.x || pixel.y >= fogSize.y)
        return;

    float2 uv = (float2(pixel) + 0.5) / float2(fogSize);

    float z01 = ForwardDepthTex.SampleLevel(LinearClampSampler, uv, 0).r;

    float3 fragPos = ReconstructWorldPos(uv, z01, u_invViewProj);

    float3 rayDir = normalize(fragPos - u_cameraPos.xyz);

    float rayDistance = length(fragPos - u_cameraPos.xyz);
    rayDistance = min(rayDistance, u_maxDistance);

    float3 radiance = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;

    float stepSize = max(u_stepSize, 0.01);
    float extinction = u_absorptionDensity + u_scatteringDensity;

    float ambientFogStrength = 0.25;

    for(float t = 0.0; t < rayDistance; t += stepSize)
    {
        float3 marchPos = u_cameraPos.xyz + rayDir * t;

        float3 ambientFog = u_sunColor.rgb
                        * ambientFogStrength
                        * u_scatteringDensity;

        radiance += transmittance * ambientFog * stepSize;
        transmittance *= exp(-extinction * stepSize);

        if(transmittance < 0.01)
            break;
    } // end for

    FogImage[pixel] = float4(radiance.xyz, transmittance);
}