#ifndef COMMON_GLSL
#define COMMON_GLSL

#define PRIMARY_RAY 0
#define SHADOW_RAY 1
#define AO_RAY 2
#define REFRACT_RAY 3
#define REFLECT_RAY 4

struct RayPayload
{
    int rayType;
    vec3 color;
    float depth;
    int shadowed;
};

#endif