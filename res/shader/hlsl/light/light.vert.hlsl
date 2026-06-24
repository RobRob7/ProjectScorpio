struct VSOutput
{
    float4 position : SV_Position;
    float2 ndc : TEXCOORD0;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    
    float2 pos;
    if (vertexID == 0)
        pos = float2(-1.0, -1.0);
    if (vertexID == 1)
        pos = float2(3.0, -1.0);
    if (vertexID == 2)
        pos = float2(-1.0, 3.0);

    output.position = float4(pos, 0.0f, 1.0f);
    output.ndc = pos;
    
    return output;
}