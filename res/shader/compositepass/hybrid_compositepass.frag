#version 460 core

layout(location = 0) in vec2 vUV;

layout(binding = 1) uniform sampler2D u_rastColorTex;
layout(binding = 2) uniform sampler2D u_rastDepthTex;
layout(binding = 3) uniform sampler2D u_rtColorTex;
layout(binding = 4) uniform sampler2D u_rtDepthTex;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec3 rastColor = texture(u_rastColorTex, vUV).rgb;
    float rastDepth = texture(u_rastDepthTex, vUV).r;

    vec3 rtColor = texture(u_rtColorTex, vUV).rgb;
    float rtDepth = texture(u_rtDepthTex, vUV).r;

    if (rtDepth >= 1.0 && rastDepth >= 1.0)
    {
        FragColor = vec4(rtColor, 1.0);
        gl_FragDepth = 1.0;
    }
    else if (rastDepth <= rtDepth)
    {
        FragColor = vec4(rastColor, 1.0);
        gl_FragDepth = rastDepth;
    }
    else
    {
        FragColor = vec4(rtColor, 1.0);
        gl_FragDepth = rtDepth;
    }
}