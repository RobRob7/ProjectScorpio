struct VSInput
{
    float3 aPos : POSITION;
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

struct VSOutput
{
    float4 position : SV_Position;
    float3 texCoords : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.texCoords = input.aPos;

    float4 worldPos = float4(input.aPos, 1.0f);
    float4 viewPos = mul(u_view, worldPos);
    float4 clipPos = mul(u_proj, viewPos);

    output.position = clipPos.xyww;

    return output;
}
