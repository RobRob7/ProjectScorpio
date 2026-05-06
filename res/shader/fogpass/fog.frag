#version 460 core

layout(location = 0) in vec2 vUV;

layout(std140, set = 0, binding = 0) uniform UBO
{
    float u_near;
    float u_far;
    vec2 _pad0;

    vec3 u_fogColor;
    float _pad1;

    float u_fogStart;
    float u_fogEnd;
    float u_ambStr;
    float _pad2;
};

layout(binding = 1) uniform sampler2D u_forwardColorTex;
layout(binding = 2) uniform sampler2D u_forwardDepthTex;

layout (location = 0) out vec4 FragColor;

float linearizeDepth(float z01)
{
    float z = z01 * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

void main()
{
    vec3 color = texture(u_forwardColorTex, vUV).rgb;

    float z01 = texture(u_forwardDepthTex, vUV).r;
    float d = linearizeDepth(z01);

    float fogFactor = clamp((u_fogEnd - d) / (u_fogEnd - u_fogStart), 0.0, 1.0);
    fogFactor = pow(fogFactor, 0.6);

    float amb = clamp(u_ambStr, 0.0, 1.0);
    float fogInfluence = mix(0.4, 1.0, amb);
    fogFactor *= fogInfluence;

    vec3 outColor = mix(u_fogColor * u_ambStr, color, fogFactor);
    FragColor = vec4(outColor, 1.0);
}
 