struct FSInput
{
    float4 position : SV_Position;
    float3 texCoords : TEXCOORD0;
};

cbuffer UBO : register(b0)
{
    // vert
    float4x4 u_view;
    float4x4 u_proj;

    // frag
    float3 _pad0;
    float u_dayNightMix;
};

TextureCube u_nightSkyboxTex : register(t1);
TextureCube u_daySkyboxTex   : register(t2);

SamplerState u_skyboxSampler : register(s0);

float4 PSMain(FSInput input) : SV_Target
{
    float3 nightTex = u_nightSkyboxTex.Sample(
        u_skyboxSampler,
        input.texCoords
    ).rgb;

    float3 dayTex = u_daySkyboxTex.Sample(
        u_skyboxSampler,
        input.texCoords
    ).rgb;

    float3 color = lerp(
        dayTex,
        nightTex,
        u_dayNightMix
    );

    return float4(color, 1.0f);
}
