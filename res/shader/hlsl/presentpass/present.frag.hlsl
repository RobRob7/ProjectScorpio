struct FSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D u_forwardColorTex : register(t1);

SamplerState u_sampler : register(s0);

float4 PSMain(FSInput input) : SV_Target
{
    float3 color = u_forwardColorTex.Sample(
        u_sampler,
        input.uv
    ).rgb;

    return float4(color, 1.0f);
}
 