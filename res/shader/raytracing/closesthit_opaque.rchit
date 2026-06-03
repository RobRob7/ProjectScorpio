#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

struct RTVertex
{
    vec4 position;
    vec4 normal;
    vec4 tileData;
};

struct RTChunkInfo
{
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uvec4 countsPad; // x = vertcount, y = idxcount, z=w=null
    vec4 chunkOrigin;
};

layout(buffer_reference, scalar) readonly buffer VertexBufferRef
{
    RTVertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer IndexBufferRef
{
    uint indices[];
};

layout(set = 2, binding = 0) uniform accelerationStructureEXT TopLevelAS;

layout(set = 2, binding = 1, scalar) readonly buffer ChunkInfoBuffer
{
    RTChunkInfo chunkInfos[];
};

layout(set = 2, binding = 2) uniform UBO
{
    vec4 u_lightDir;
    vec4 u_lightColor;

    float u_ambStr;
} ubo;

layout(set = 2, binding = 3) uniform sampler2D AtlasTex;
layout(set = 2, binding = 4) uniform sampler2D RTAOTex;
layout(set = 2, binding = 5) uniform sampler2D RTShadowTex;

// ------------------------------------------------------------
// Constants
// ------------------------------------------------------------
const float ATLAS_COLS = 32.0;
const float ATLAS_ROWS = 32.0;

const float INNER_TILE_SIZE  = 16.0;
const float PADDED_TILE_SIZE = 32.0;
const float PADDING          = 8.0;
const float INSET            = 0.0;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
vec2 atlasUV(uvec2 tile, vec2 local01)
{
    vec2 atlasPixelSize = vec2(
        ATLAS_COLS * PADDED_TILE_SIZE,
        ATLAS_ROWS * PADDED_TILE_SIZE
    );

    vec2 cellOriginPx  = vec2(tile) * PADDED_TILE_SIZE;
    vec2 innerOriginPx = cellOriginPx + vec2(PADDING);

    vec2 innerSpan = vec2(INNER_TILE_SIZE - 1.0 - 2.0 * INSET);

    vec2 innerUVPx =
        innerOriginPx +
        vec2(0.5 + INSET) +
        local01 * innerSpan;

    return innerUVPx / atlasPixelSize;
}

void main()
{
//    // shadow ray
//    if (payload.rayType == 1)
//    {
//        payload.shadowed = 1;
//        return;
//    }

    payload.depth = gl_HitTEXT;

    uint chunkIndex = gl_InstanceCustomIndexEXT;
    RTChunkInfo info = chunkInfos[chunkIndex];

    VertexBufferRef vbuf = VertexBufferRef(info.vertexAddress);
    IndexBufferRef  ibuf = IndexBufferRef(info.indexAddress);

    uint triBase = gl_PrimitiveID * 3u;

    if (triBase + 2u >= info.countsPad.y)
    {
        payload.color = vec3(1.0, 0.0, 0.0);
        return;
    }

    uint i0 = ibuf.indices[triBase + 0u];
    uint i1 = ibuf.indices[triBase + 1u];
    uint i2 = ibuf.indices[triBase + 2u];

    uint vertexCount = info.countsPad.x;
    if (i0 >= vertexCount ||
        i1 >= vertexCount ||
        i2 >= vertexCount)
    {
        payload.color = vec3(1.0, 1.0, 0.0);
        return;
    }

    RTVertex v0 = vbuf.vertices[i0];
    RTVertex v1 = vbuf.vertices[i1];
    RTVertex v2 = vbuf.vertices[i2];

    vec3 bary;
    bary.y = attribs.x;
    bary.z = attribs.y;
    bary.x = 1.0 - bary.y - bary.z;

    vec3 localHitPos =
        v0.position.xyz * bary.x +
        v1.position.xyz * bary.y +
        v2.position.xyz * bary.z;

    vec3 worldHitPos = localHitPos + info.chunkOrigin.xyz;

    vec3 normal = normalize(
        v0.normal.xyz * bary.x +
        v1.normal.xyz * bary.y +
        v2.normal.xyz * bary.z
    );
    
//    payload.color = normal * 0.5 + 0.5;
//    return;

    vec2 tiled;
    if (abs(normal.x) > 0.9)
        tiled = worldHitPos.zy;
    else if (abs(normal.y) > 0.9)
        tiled = worldHitPos.xz;
    else
        tiled = worldHitPos.xy;

    vec2 local = fract(tiled);

    uvec2 tile = uvec2(v0.tileData.xy);
    vec2 uv = atlasUV(tile, local);

    vec4 texColor = texture(AtlasTex, uv);

    vec3 lightdir = normalize(-ubo.u_lightDir.xyz);
    float ndotl = max(dot(normal, lightdir) , 0.0);

    vec2 screenUV = (vec2(gl_LaunchIDEXT.xy) + 0.5) / vec2(gl_LaunchSizeEXT.xy);
    // AO
    float ao = texture(RTAOTex, screenUV).r;
    float aoDirect = mix(1.0, ao, 0.5);
    // shadow
    float shadow = texture(RTShadowTex, screenUV).r;

    payload.rayType = 0;

    vec3 ambient = ubo.u_lightColor.rgb * ubo.u_ambStr * texColor.rgb * aoDirect;
    vec3 diffuse = texColor.rgb * ubo.u_lightColor.rgb * ndotl * shadow;

    payload.color = ambient + diffuse;
}