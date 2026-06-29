struct PSInput
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD3;
};

struct PSOutput
{
    float4 Color : COLOR;
};

PSOutput PSMain(PSInput input) : SV_Target0
{
    PSOutput output;
    
    output.Color = float4(normalize(input.Normal), 1.0);
    
    return output;
}
