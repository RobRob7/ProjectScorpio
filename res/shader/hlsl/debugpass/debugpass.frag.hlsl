struct PSInput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

cbuffer UBO : register(b0, space0)
{
    int u_mode;
    float u_near;
    float u_far;
    float _pad0;
}; 

Texture2D u_gNormal : register(t1, space0);
Texture2D u_gDepth : register(t2, space0);
Texture2D u_shadowTex : register(t3, space0);
Texture2D u_rtDepthTex : register(t4, space0);

SamplerState u_sampler : register(s1, space0);

struct PSOutput
{
    float4 Color : SV_Target0;
};

float LinearizeDepth(float z01)
{
    return (u_near * u_far) /
           (u_far - z01 * (u_far - u_near));
} // end of LinearizeDepth()

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    
    // default
    output.Color = float4(1.0, 0.0, 1.0, 1.0);
    
    // normal
    if (u_mode == 1)
    {
        float3 n = u_gNormal.Sample(u_sampler, input.UV).rgb;
        output.Color = float4(n * 0.5 + 0.5, 1.0);
    }

    // depth
    if (u_mode == 2)
    {
        float d = u_gDepth.Sample(u_sampler, input.UV).r;
        float lin = LinearizeDepth(d);

        // visualize depth
        float vis = saturate(lin / 100.0);
        output.Color = float4(vis.xxx, 1.0);
    }

    // shadow map
    if (u_mode == 3)
    {
        float d = u_shadowTex.Sample(u_sampler, input.UV).r;
        float vis = 1.0 - d;
        output.Color = float4(float3(vis, vis, vis), 1.0);
    }

    // RT depth
    if (u_mode == 4)
    {
        float d = u_rtDepthTex.Sample(u_sampler, input.UV).r;

        // miss pixels
        if (d > 1e29)
        {
            output.Color = float4(0.0, 0.0, 0.0, 1.0);
        }
        else
        {
            // visualize ray distance
            float vis = clamp(d / u_far, 0.0, 1.0);
            output.Color = float4(float3(vis, vis, vis), 1.0);
        }
    }
    
    return output;
}
