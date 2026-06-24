cbuffer UBO : register(b0, space0)
{
    float4x4 u_invViewProj;
    float4x4 u_viewProj;
		
    float3 u_camPos;
    float u_sunDistance;

    float4 u_lightPos;

    float3 u_lightVisualColor;
    float u_sunRadius;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 ndc : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_Target0;
    float depth : SV_Depth;
};

float SdSphere(float3 p, float r)
{
    return length(p) - r;
} // end of SdSphere()

float3 GetRayDir(float2 ndc)
{
    float4 nearPoint = mul(u_invViewProj, float4(ndc, 0.0, 1.0));
    float4 farPoint = mul(u_invViewProj, float4(ndc, 1.0, 1.0));

    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;

    return normalize(farPoint.xyz - nearPoint.xyz);
} // end of GetRayDir()

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    
    float3 rayOrigin = u_camPos;
    float3 rayDir = GetRayDir(input.ndc);

    float travel = 0.0;
    bool hit = false;
    for (int i = 0; i < 64; ++i)
    {
        float3 pointOnRay = rayOrigin + rayDir * travel;

        float distanceToObj = SdSphere(pointOnRay - u_lightPos.xyz, u_sunRadius);

        // ray touched surface
        if(distanceToObj < 0.01)
        {
            hit = true;
            break;
        }

        travel += distanceToObj;

        // check if traveled beyond possible area
        if(travel > u_sunDistance + u_sunRadius)
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
    float3 hitPos = rayOrigin + rayDir * travel;
    float4 clip = mul(u_viewProj, float4(hitPos, 1.0));
    float depth = clip.z / clip.w;
    output.depth = depth;

    // sun color
    output.color = float4(u_lightVisualColor, 1.0);
    
    return output;
}
