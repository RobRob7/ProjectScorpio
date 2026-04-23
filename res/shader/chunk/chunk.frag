#version 460 core

layout (location = 0) flat in uvec2 Tile;
layout (location = 1) in vec2 TileUV;
layout (location = 2) in vec3 FragWorldPos;
layout (location = 3) in vec3 Normal;
layout (location = 4) in vec4 FragPosLightSpace;

layout (std140, set = 0, binding = 0) uniform UBO
{
    // vert
    mat4 u_lightSpaceMatrix;

    vec3 u_chunkOrigin;
    float _pad0;

    mat4 u_view;
    mat4 u_proj;

    vec4 u_clipPlane;

    // frag
    vec3 u_viewPos;
    float _pad1;

    vec3 u_lightDir;
    float _pad2;

    vec3 u_lightColor;
    float u_ambientStrength;

    vec2 u_screenSize;
    int u_useSSAO;
    int u_useShadowMap;
};

layout (binding = 1) uniform sampler2D u_atlasTex;
layout (binding = 2) uniform sampler2D u_ssaoRaw;
layout (binding = 3) uniform sampler2D u_shadowTex;

layout(location = 0) out vec4 FragColor;

const float ATLAS_COLS = 32.0;
const float ATLAS_ROWS = 32.0;

const float INNER_TILE_SIZE = 16.0;
const float PADDED_TILE_SIZE = 32.0;
const float PADDING = 8.0;
const float INSET = 0.0;

vec2 atlasUV(uvec2 tile, vec2 local01)
{
    vec2 atlasPixelSize = vec2(
        ATLAS_COLS * PADDED_TILE_SIZE,
        ATLAS_ROWS * PADDED_TILE_SIZE
    );

    vec2 cellOriginPx = vec2(tile) * PADDED_TILE_SIZE;
    vec2 innerOriginPx = cellOriginPx + vec2(PADDING);

    vec2 innerSpan = vec2(INNER_TILE_SIZE - 1.0 - 2.0 * INSET);

    vec2 innerUVPx = innerOriginPx
                   + vec2(0.5 + INSET)
                   + local01 * innerSpan;

    return innerUVPx / atlasPixelSize;
} // end of atlasUV()

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    #ifdef VULKAN
        projCoords.xy = projCoords.xy * 0.5 + 0.5;
    #else
        projCoords = projCoords * 0.5 + 0.5;
    #endif

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 0.0;
    }

    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(-u_lightDir);

    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005); 

    float currentDepth = projCoords.z;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_shadowTex, 0));
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(u_shadowTex, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        } // end for
    } // end for

    return shadow / 9.0;
} // end of ShadowCalculation()

void main()
{
    // get correct UV (due to greedy meshing)
    vec2 tiled = TileUV;
    vec2 local = fract(tiled);

    vec2 uv = atlasUV(Tile, local);

    vec2 atlasPixelSize = vec2(
        ATLAS_COLS * PADDED_TILE_SIZE,
        ATLAS_ROWS * PADDED_TILE_SIZE
    );

    vec2 innerSpan = vec2(INNER_TILE_SIZE - 1.0 - 2.0 * INSET);
    vec2 uvScale = innerSpan / atlasPixelSize;

    vec2 dudx = dFdx(tiled) * uvScale;
    vec2 dudy = dFdy(tiled) * uvScale;

    vec4 texColor = textureGrad(u_atlasTex, uv, dudx, dudy);

    // allow textures to be see-through
    // (like tree canopy texture)
    if (texColor.a < 0.1)
    {
        discard;
    }

    // SSAO
    float ao = 1.0;
    if (u_useSSAO != 0)
    {
        vec2 ssUV = gl_FragCoord.xy / u_screenSize;
        ao = texture(u_ssaoRaw, ssUV).r;
    }

    // ambient
    vec3 ambient = u_lightColor * u_ambientStrength * texColor.rgb * ao;

    // diffuse
    vec3 lightDir = normalize(-u_lightDir);
    vec3 normal = normalize(Normal);
    float diff = max(dot(normal, lightDir), 0.0);
    // fake face shading
    if (normal.y > 0.9) diff *= 1.0;      // top
    else if (normal.y < -0.9) diff *= 0.4; // bottom
    else diff *= 0.7;                      // sides
    vec3 diffuse = u_lightColor * diff * texColor.rgb * 0.8;

    // specular
    vec3 specular = vec3(0.0);

    // shadow calc
    float shadowFactor = 1.0;
    if (u_useShadowMap != 0)
    {
        float shadow = ShadowCalculation(FragPosLightSpace);
        shadowFactor = clamp(1.0 - shadow, 0.0, 1.0);
    }

    float aoDirect = mix(1.0, ao, 0.5);
    vec3 direct = (diffuse + specular) * aoDirect;
    // final color
    vec3 color = (ambient + (shadowFactor * direct));
    FragColor = vec4(color, 1.0);
}
