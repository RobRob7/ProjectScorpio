#version 430 core

layout (location = 0) in vec3 TexCoords;

layout (std140, set = 0, binding = 0) uniform UBO
{
    // vert
    mat4 u_view;
    mat4 u_proj;

    // frag
    vec3 _pad0;
    float u_dayNightMix;
};

layout (binding = 1) uniform samplerCube u_nightSkyboxTex;
layout (binding = 2) uniform samplerCube u_daySkyboxTex;

layout (location = 0) out vec3 FragColor;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
} // end of hash()

void main()
{
    vec3 nightTex = texture(u_nightSkyboxTex, TexCoords).rgb;
    vec3 dayTex = texture(u_daySkyboxTex, TexCoords).rgb;

    vec3 color = mix(dayTex, nightTex, u_dayNightMix);

    float noise = hash(gl_FragCoord.xy) - 0.5;
    color += noise / 255.0;
    
    FragColor = color;
}

