#version 460 core

layout(location = 0) in vec2 vUV;

layout(location = 0) out vec4 FragColor;

layout(std140, set = 0, binding = 0) uniform UBO
{
    	mat4 u_invViewProj;
        mat4 u_viewProj;
		
		vec3 u_camPos;
		float u_sunDistance;

        vec4 u_lightPos;

		vec3 u_lightVisualColor;
		float u_sunRadius;
} ubo;

float sdSphere(vec3 p, float r)
{
    return length(p) - r;
} // end of sdSphere()

vec3 getRayDir(vec2 uv)
{
    vec2 ndc = uv * 2.0 - 1.0;

    vec4 nearPoint = ubo.u_invViewProj * vec4(ndc, 0.0, 1.0);
    vec4 farPoint  = ubo.u_invViewProj * vec4(ndc, 1.0, 1.0);

    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;

    return normalize(farPoint.xyz - nearPoint.xyz);
} // end of getRayDir()

void main()
{
    vec3 rayOrigin = ubo.u_camPos;
    vec3 rayDir = getRayDir(vUV);

    float travel = 0.0;
    bool hit = false;
    for (int i = 0; i < 64; ++i)
    {
        vec3 pointOnRay = rayOrigin + rayDir * travel;

        float distanceToObj = sdSphere(pointOnRay - ubo.u_lightPos.xyz, ubo.u_sunRadius);

        // ray touched surface
        if(distanceToObj < 0.01)
        {
            hit = true;
            break;
        }

        travel += distanceToObj;

        // check if traveled beyond possible area
        if(travel > ubo.u_sunDistance + ubo.u_sunRadius)
        {
            break;
        }
    } // end for

    // no hit - transparent
    if(!hit)
    {
        discard;
    }

    // calculate depth
    vec3 hitPos = rayOrigin + rayDir * travel;
    vec4 clip = ubo.u_viewProj * vec4(hitPos, 1.0);
    float depth = clip.z / clip.w;
    gl_FragDepth = depth;

    // sun color
    FragColor = vec4(ubo.u_lightVisualColor, 1.0);
}
