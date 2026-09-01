struct VSInput
{
    uint combined : POSITION;
};

cbuffer UBO : register(b0, space0)
{
    float4x4 u_lightSpaceMatrix;
    
    float3 u_chunkOrigin;
    float _pad0;
};

cbuffer ChunkPushConstants : register(b0, space1)
{
    float4 pc_u_chunkOrigin;
};

struct VSOutput
{
    float4 position : SV_Position;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    uint combinedCopy = input.combined;

    // skip uv index, tileX, tileY
    combinedCopy >>= 12;
    // derive normal index
    uint n = combinedCopy & 7u;
    n = min(n, 5u);
    combinedCopy >>= 3;

    float3 aPos;
    // derive aPos.x
    aPos.x = float(combinedCopy & 15);
    combinedCopy >>= 4;
    // derive aPos.y
    aPos.y = float(combinedCopy & 511);
    combinedCopy >>= 9;
    // derive aPos.z
    aPos.z = float(combinedCopy & 15);
    combinedCopy >>= 4;

    float3 world = aPos + pc_u_chunkOrigin.xyz;

    output.position = mul(u_lightSpaceMatrix, float4(world, 1.0));
    
    return output;
}


