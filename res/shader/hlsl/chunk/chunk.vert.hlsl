struct VSInput
{
    uint combined : POSITION;
};

cbuffer UBO : register(b0, space0)
{
    // vert
    float4x4 u_lightSpaceMatrix;

    float3 u_chunkOrigin;
    float _pad0;

    float4x4 u_view;
    float4x4 u_proj;

    float4 u_clipPlane;

    // frag
    float3 u_viewPos;
    float _pad1;

    float3 u_lightDir;
    float _pad2;

    float3 u_lightColor;
    float u_ambientStrength;

    float2 u_screenSize;
    int u_useSSAO;
    int u_useShadowMap;
};

cbuffer ChunkPushConstants : register(b0, space1)
{
    float4 pc_u_chunkOrigin;
};

struct VSOutput
{ 
    float4 Position : SV_Position;

    nointerpolation uint2 Tile : TEXCOORD0;
    float2 TileUV : TEXCOORD1;
    float3 FragWorldPos : TEXCOORD2;
    float3 Normal : TEXCOORD3;
    float4 FragPosLightSpace : TEXCOORD4;

    float ClipDistance : SV_ClipDistance0;
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

    // derive uv corner index
    uint uvIndex = combinedCopy & 3;
    combinedCopy >>= 2;

    // derive tileY
    uint tileY = combinedCopy & 31;
    combinedCopy >>= 5;

    // derive tileX
    uint tileX = combinedCopy & 31;
    combinedCopy >>= 5;

    // derive normal index
    uint normalIndex = combinedCopy & 7;
    output.Normal = normalSample[normalIndex];
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

    output.Tile = uint2(tileX, tileY);

    float3 world = aPos + pc_u_chunkOrigin.xyz;

    float4 worldPos = float4(world, 1.0);
    output.FragWorldPos = world;

    // X faces use (z,y)
    if (normalIndex == 0u || normalIndex == 1u)
    {
        output.TileUV = world.zy;
    }
    // Y faces use (x,z)
    else if (normalIndex == 2u || normalIndex == 3u)
    {
        output.TileUV = world.xz;
    }
    // Z faces use (x,y)
    else
    {
        output.TileUV = world.xy;
    }

    output.FragPosLightSpace = mul(
        u_lightSpaceMatrix,
        float4(output.FragWorldPos, 1.0)
    );

    output.ClipDistance = dot(worldPos, u_clipPlane);

    float4 viewPos = mul(u_view, worldPos);
    output.Position = mul(u_proj, viewPos);

    return output;
}