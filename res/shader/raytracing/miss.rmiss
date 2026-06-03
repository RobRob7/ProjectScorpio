#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout (std140, set = 1, binding = 0) uniform UBO
{
    vec4 u_mix;
};

layout(set = 1, binding = 1) uniform samplerCube u_nightSkyboxTex;
layout(set = 1, binding = 2) uniform samplerCube u_daySkyboxTex;

void main()
{
    vec3 dir = normalize(gl_WorldRayDirectionEXT);

    vec3 nightTex = texture(u_nightSkyboxTex, dir).rgb;
    vec3 dayTex = texture(u_daySkyboxTex, dir).rgb;

    vec3 skyColor = mix(dayTex, nightTex, u_mix.x);
    
    payload.color = skyColor;
    payload.depth = 1e30;
}