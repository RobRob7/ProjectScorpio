#version 460 core

layout(location = 0) in vec2 vUV;

layout(binding = 1) uniform sampler2D FogTex;
layout(binding = 2) uniform sampler2D GodRayTex;
layout(binding = 3) uniform sampler2D SceneColorTex;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec4 fogColor = texture(FogTex, vUV);
	vec4 godRayColor = texture(GodRayTex, vUV);
    vec3 sceneColor = texture(SceneColorTex, vUV).rgb;

	vec3 finalColor = sceneColor * fogColor.a + fogColor.rgb;
	finalColor += godRayColor.rgb;

	FragColor = vec4(finalColor, 1.0);
}