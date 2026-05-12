#version 460 core

layout(location = 0) in vec2 vUV;

layout(binding = 1) uniform sampler2D FogColorTex;
layout(binding = 2) uniform sampler2D SceneColorTex;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec4 fogColor = texture(FogColorTex, vUV);
	vec3 radiance = fogColor.rgb;
	float transmittance = fogColor.a;

    vec3 sceneColor = texture(SceneColorTex, vUV).rgb;

	FragColor = vec4(sceneColor * transmittance + radiance, 1.0);
}