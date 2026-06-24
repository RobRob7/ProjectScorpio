#include "../helper.hlsl"

cbuffer UBO : register(b0)
{
    float4x4 u_invViewProj;
    float4x4 u_lightSpaceMatrix;

    float4 u_cameraPos;
    float4 u_sunColor;

    float3 u_lightDir;
    float u_maxDistance;

    float u_stepSize;
    float3 _pad0;
};

Texture2D<float> ForwardDepthTex : register(t1);
Texture2D<float> ShadowMapTex : register(t2);
RWTexture2D<float4> GodRaysImage : register(u3);

SamplerState LinearClampSampler : register(s0);

float SampleShadowMap(float3 worldPos)
{
    float4 lightClip = mul(u_lightSpaceMatrix, float4(worldPos, 1.0));
    float3 proj = lightClip.xyz / lightClip.w;

// #ifdef VULKAN
//     proj.xy = proj.xy * 0.5 + 0.5;
// #else
//     proj = proj * 0.5 + 0.5;
// #endif

    proj.xy = proj.xy * 0.5 + 0.5;

    if(proj.x < 0.0 || proj.x > 1.0 ||
       proj.y < 0.0 || proj.y > 1.0 ||
       proj.z < 0.0 || proj.z > 1.0)
    {
        return 1.0;
    }

    float closestDepth = ShadowMapTex.SampleLevel(LinearClampSampler, proj.xy, 0).r;
    float currentDepth = proj.z;

    return currentDepth <= closestDepth ? 1.0 : 0.0;
} // end of SampleShadowMap()

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pixel = int2(dispatchThreadID.xy);

    uint godRayWidth;
    uint godRayHeight;
    GodRaysImage.GetDimensions(godRayWidth, godRayHeight);

    int2 godRaySize = int2(godRayWidth, godRayHeight); 

    if(pixel.x >= godRaySize.x || pixel.y >= godRaySize.y)
        return;

    float2 uv = (float2(pixel) + 0.5) / float2(godRaySize);

    float z01 = ForwardDepthTex.SampleLevel(LinearClampSampler, uv, 0).r;

    float3 fragPos = ReconstructWorldPos(uv, z01, u_invViewProj);

    float3 rayDir = normalize(fragPos - u_cameraPos.xyz);

    float rayDistance = length(fragPos - u_cameraPos.xyz);
    rayDistance = min(rayDistance, u_maxDistance);

    float3 radiance = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;

    float stepSize = max(u_stepSize, 0.01);

    float godRayDensity = 0.003;
    float godRayIntensity = 3.0;
    float extinction = godRayDensity;

    float sunAmount = max(-u_lightDir.y, 0.0);

    for (float t = 0.0; t < rayDistance; t += stepSize)
    {
        float3 marchPos = u_cameraPos.xyz + rayDir * t;

        float visibility = SampleShadowMap(marchPos);

        float3 sunScatter = u_sunColor.rgb
                        * visibility
                        * sunAmount
                        * godRayDensity
                        * godRayIntensity;

        radiance += transmittance * sunScatter * stepSize;
        transmittance *= exp(-extinction * stepSize);

        if(transmittance < 0.01)
            break;
    } // end for

    GodRaysImage[pixel] = float4(radiance, 1.0f);
}
