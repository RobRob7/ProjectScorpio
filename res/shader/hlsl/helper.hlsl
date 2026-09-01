#ifndef HELPER_HLSL
#define HELPER_HLSL

float LinearizeDepth(float z01, float nearPlane, float farPlane)
{
    return (nearPlane * farPlane) /
           (farPlane - z01 * (farPlane - nearPlane));
} // end of LinearizeDepth()

float3 ReconstructViewPos(float2 uv, float z, float4x4 invProj)
{
    float2 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = 1.0f - uv.y * 2.0f;

    float4 clip = float4(ndc, z, 1.0f);

    float4 view = mul(invProj, clip);
    return view.xyz / view.w;
} // end of ReconstructViewPos()

float3 ReconstructWorldPos(float2 uv, float z, float4x4 invViewProj)
{
    float2 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = 1.0f - uv.y * 2.0f;

    float4 clip = float4(ndc, z, 1.0f);

    float4 worldPos = mul(invViewProj, clip);
    return worldPos.xyz / worldPos.w;
} // end of reconstructWorldPos()

#endif
