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

Texture2D u_atlasTex : register(t1, space0);
Texture2D u_ssaoRaw : register(t2, space0);
Texture2D u_shadowTex : register(t3, space0);

SamplerState u_sampler : register(s0, space0);

struct PSInput
{
    float4 Position : SV_Position;

    nointerpolation uint2 Tile : TEXCOORD0;
    float2 TileUV : TEXCOORD1;
    float3 FragWorldPos : TEXCOORD2;
    float3 Normal : TEXCOORD3;
    float4 FragPosLightSpace : TEXCOORD4;
};

static const float ATLAS_COLS = 32.0;
static const float ATLAS_ROWS = 32.0;

static const float INNER_TILE_SIZE = 16.0;
static const float PADDED_TILE_SIZE = 32.0;
static const float PADDING = 8.0;
static const float INSET = 0.0;

float2 atlasUV(uint2 tile, float2 local01)
{
    float2 atlasPixelSize = float2(
        ATLAS_COLS * PADDED_TILE_SIZE,
        ATLAS_ROWS * PADDED_TILE_SIZE
    );

    float2 cellOriginPx = float2(tile) * PADDED_TILE_SIZE;
    float2 innerOriginPx = cellOriginPx + float2(PADDING, PADDING);

    float2 innerSpan = float2(
        INNER_TILE_SIZE - 1.0 - 2.0 * INSET,
        INNER_TILE_SIZE - 1.0 - 2.0 * INSET
    );

    float2 innerUVPx =
        innerOriginPx +
        float2(0.5 + INSET, 0.5 + INSET) +
        local01 * innerSpan;

    return innerUVPx / atlasPixelSize;
}

float ShadowCalculation(float4 fragPosLightSpace, float3 normalInput)
{
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    float2 shadowUV;
    shadowUV.x = projCoords.x * 0.5 + 0.5;
    shadowUV.y = -projCoords.y * 0.5 + 0.5;

    float currentDepth = projCoords.z;

    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 ||
        shadowUV.y < 0.0 || shadowUV.y > 1.0 ||
        currentDepth < 0.0 || currentDepth > 1.0)
    {
        return 0.0;
    }

    float3 normal = normalize(normalInput);
    float3 lightDir = normalize(-u_lightDir);

    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    uint shadowWidth;
    uint shadowHeight;
    u_shadowTex.GetDimensions(shadowWidth, shadowHeight);

    float2 texelSize = 1.0 / float2(shadowWidth, shadowHeight);

    float shadow = 0.0;

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = u_shadowTex.Sample(
                u_sampler,
                shadowUV + float2(x, y) * texelSize
            ).r;

            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
} // end of ShadowCalculation()

float4 PSMain(PSInput input) : SV_Target0
{
    // get correct UV due to greedy meshing
    float2 tiled = input.TileUV;
    float2 local = frac(tiled);

    float2 uv = atlasUV(input.Tile, local);

    float2 atlasPixelSize = float2(
        ATLAS_COLS * PADDED_TILE_SIZE,
        ATLAS_ROWS * PADDED_TILE_SIZE
    );

    float2 innerSpan = float2(
        INNER_TILE_SIZE - 1.0 - 2.0 * INSET,
        INNER_TILE_SIZE - 1.0 - 2.0 * INSET
    );

    float2 uvScale = innerSpan / atlasPixelSize;

    float2 dudx = ddx(tiled) * uvScale;
    float2 dudy = ddy(tiled) * uvScale;

    float4 texColor = u_atlasTex.SampleGrad(
        u_sampler,
        uv,
        dudx,
        dudy
    );

    // allow textures to be see-through
    if (texColor.a < 0.5)
    {
        discard;
    }

    // SSAO
    float ao = 1.0;

    if (u_useSSAO != 0)
    {
        float2 ssUV = input.Position.xy / u_screenSize;
        ao = u_ssaoRaw.Sample(u_sampler, ssUV).r;
    }

    // ambient
    float3 ambient =
        u_lightColor *
        u_ambientStrength *
        texColor.rgb *
        ao;

    // diffuse
    float3 lightDir = normalize(-u_lightDir);
    float3 normal = normalize(input.Normal);

    float diff = max(dot(normal, lightDir), 0.0);

    // fake face shading
    if (normal.y > 0.9)
    {
        diff *= 1.0;
    }
    else if (normal.y < -0.9)
    {
        diff *= 0.4;
    }
    else
    {
        diff *= 0.7;
    }

    float3 diffuse =
        u_lightColor *
        diff *
        texColor.rgb *
        0.8;

    // specular
    float3 specular = float3(0.0, 0.0, 0.0);

    // shadow
    float shadowFactor = 1.0;

    if (u_useShadowMap != 0)
    {
        float shadow = ShadowCalculation(
            input.FragPosLightSpace,
            input.Normal
        );

        shadowFactor = clamp(1.0 - shadow, 0.0, 1.0);
    }

    float aoDirect = lerp(1.0, ao, 0.5);
    float3 direct = (diffuse + specular) * aoDirect;

    float3 color = ambient + shadowFactor * direct;

    return float4(color, 1.0);
}