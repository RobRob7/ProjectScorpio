#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../common.glsl"

layout(location = 0) rayPayloadInEXT RTShadowPayload payload;

void main()
{
	payload.shadowed = false;
}