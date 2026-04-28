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

layout(set = 3, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(set = 3, binding = 1, scalar) readonly buffer ChunkInfoBuffer
{
    RTChunkInfo chunkInfos[];
};

layout(set = 3, binding = 2) uniform UBO
{
    vec4 u_lightDir;
    vec4 u_lightColor;

    float u_time;
} ubo;

layout(set = 3, binding = 3) uniform sampler2D u_dudvTex;
layout(set = 3, binding = 4) uniform sampler2D u_normalTex;

void main()
{
	// shadow ray
    if (payload.rayType == 1)
    {
        payload.shadowed = 1;
        return;
    }

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

    ///////////
    vec3 baseNormal = normalize(
        v0.normal.xyz * bary.x +
        v1.normal.xyz * bary.y +
        v2.normal.xyz * bary.z
    );

    float time = ubo.u_time;
    float waveSpeed = 0.04f;
    float uvScale = 0.02f;
    float normalStrength =  0.45f;

    vec2 baseUV = worldHitPos.xz * uvScale;

    vec2 dudvUV1 = fract(baseUV + vec2(time * waveSpeed, time * waveSpeed * 0.5));
    vec2 dudvUV2 = fract(baseUV * 1.73 + vec2(-time * waveSpeed * 0.6, time * waveSpeed * 0.9));

    vec2 distortion1 = texture(u_dudvTex, dudvUV1).rg * 2.0 - 1.0;
    vec2 distortion2 = texture(u_dudvTex, dudvUV2).rg * 2.0 - 1.0;

    float mixFactor = 0.7;
    vec2 dudv = mix(distortion1, distortion2, mixFactor);

    vec2 nUV1 = fract(dudvUV1 + dudv * 0.05);
    vec2 nUV2 = fract(dudvUV2 + dudv * 0.05);

    vec3 n1 = texture(u_normalTex, nUV1).rgb;
    vec3 n2 = texture(u_normalTex, nUV2).rgb;
    vec3 nTex = mix(n1, n2, mixFactor);

    vec3 N = vec3(
        nTex.r * 2.0 - 1.0,
        nTex.b,
        nTex.g * 2.0 - 1.0
    );

    N = normalize(N);

    vec3 waterNormal = N;

    ////////
    vec3 I = normalize(gl_WorldRayDirectionEXT);
    vec3 viewDir = normalize(-I);

    // --------------------
    // Fresnel
    // --------------------
    // Detect which side of the water surface the ray is coming from
    bool underwaterView = dot(I, baseNormal) > 0.0;

    // Use the correct normal and IOR depending on side
    vec3 Nuse = waterNormal;
    float eta = 1.0 / 1.333; // air -> water

    if (underwaterView)
    {
        Nuse = -waterNormal;
        eta = 1.333 / 1.0; // water -> air
    }

    // Fresnel
    float ndv = clamp(abs(dot(Nuse, viewDir)), 0.0, 1.0);
    float fresnel = pow(1.0 - ndv, 5.0);

    if (underwaterView)
    {
        fresnel = mix(0.45, 1.0, fresnel);
    }
    else
    {
        fresnel = mix(0.2, 0.98, fresnel);
    }


    // --------------------
    // Refraction ray
    // --------------------
    vec3 refractDir = refract(I, Nuse, eta);

    if (length(refractDir) < 0.001)
    {
        refractDir = I;
    }

    // opposite side of surface
    vec3 refractOrigin = underwaterView
        ? worldHitPos + Nuse * 0.05
        : worldHitPos - Nuse * 0.05;

    payload.rayType = 2;
    payload.color = vec3(0.0, 0.18, 0.35);
    payload.depth = 1e30;

    traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT,
        0x01,        // opaque terrain/chunks only
        0, 0, 0,
        refractOrigin,
        0.01,
        normalize(refractDir),
        10000.0,
        0
    );

    vec3 refractedColor = payload.color;


    // --------------------
    // Reflection ray
    // --------------------
    vec3 flatN = normalize(mix(Nuse, vec3(0.0, sign(Nuse.y), 0.0), 0.9));
    vec3 reflectDir = reflect(I, flatN);
    vec3 reflectOrigin = worldHitPos + Nuse * 0.05;

    payload.rayType = 3;
    payload.color = vec3(0.35, 0.55, 0.75); // sky fallback if miss shader does not set it
    payload.depth = 1e30;

    traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT,
        0x01,
        0, 0, 0,
        reflectOrigin,
        0.01,
        normalize(reflectDir),
        10000.0,
        0
    );

    vec3 reflectedColor = payload.color * 1.35;


    // restore payload
    payload.rayType = 0;
    payload.depth = gl_HitTEXT;
    ////////

    vec3 lightDir = normalize(-ubo.u_lightDir.xyz);
    float ndotl = max(dot(Nuse, lightDir), 0.0);

    vec3 shadowOrigin = worldHitPos + Nuse * 0.03;
    ///////////

    payload.shadowed = 0;
    payload.rayType = 1;

    traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
        0x01,
        0, 0, 0,
        shadowOrigin,
        0.001,
        lightDir,
        10000.0,
        0
    );

    float shadow = (payload.shadowed != 0) ? 0.25 : 1.0;

    payload.rayType = 0;

    vec3 waterTint = vec3(0.0, 0.18, 0.45);
    vec3 underwaterTint = vec3(0.0, 0.20, 0.35);

    // Make refraction show more clearly
    refractedColor = mix(refractedColor, underwaterTint, 0.15);


    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(Nuse, halfDir), 0.0), 96.0);

    vec3 ambient = waterTint * 0.08;
    vec3 diffuse = waterTint * ubo.u_lightColor.rgb * ndotl * 0.18 * shadow;
    vec3 specular = ubo.u_lightColor.rgb * spec * 0.45 * shadow;

    vec3 surfaceLighting = ambient + diffuse + specular;

    vec3 underwater = refractedColor * (1.0 - fresnel);

    vec3 surface = reflectedColor * fresnel;
    surface += surfaceLighting;
    surface *= mix(0.45, 1.0, shadow);

    vec3 waterColor = underwater + surface;

    payload.color = waterColor;
}