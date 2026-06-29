struct VSInput
{
    uint combined : POSITION;
};

cbuffer UBO : register(b0, space0)
{
	float4x4 u_view;
    float4x4 u_proj;

	float3 u_chunkOrigin;
	float _pad0;
};

cbuffer ChunkPushConstants : register(b0, space1)
{
    float4 pc_u_chunkOrigin;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Normal : TEXCOORD3;
};

static const float3 normalSample[6] =
{
    float3(1.0, 0.0, 0.0),
    float3(-1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, -1.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(0.0, 0.0, -1.0)
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
    output.Normal = normalize(mul(u_view, float4(normalSample[n], 0.0))).xyz;
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
    float4 worldPos = float4(world, 1.0f);
    
    float4 viewPos = mul(u_view, worldPos);
    
    output.Position = mul(u_proj, viewPos);
    
    return output;
}


