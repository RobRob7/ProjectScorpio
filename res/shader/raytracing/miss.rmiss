#version 460 core
#extension GL_EXT_ray_tracing : require

layout (location = 0) rayPayloadInEXT vec3 payloadColor;

void main()
{
    payloadColor = vec3(1.0, 0.0, 1.0);
}
