#ifndef HELPER_HLSL
#define HELPER_HLSL

float LinearizeDepth(float z01, float nearPlane, float farPlane)
{
    return (nearPlane * farPlane) /
           (farPlane - z01 * (farPlane - nearPlane));
} // end of LinearizeDepth()

float3 ReconstructViewPos(float2 uv, float z01, float4x4 invProj)
{
    float z = z01;

    float4 clip = float4(uv * 2.0 - 1.0, z, 1.0);

    float4 view = mul(invProj, clip);
    return view.xyz / view.w;
} // end of ReconstructViewPos()

float3 ReconstructWorldPos(float2 uv, float z01, float4x4 invViewProj)
{
    float z = z01;

    float4 clip = float4(uv * 2.0 - 1.0, z, 1.0);

    float4 worldPos = mul(invViewProj, clip);
    return worldPos.xyz / worldPos.w;
} // end of reconstructWorldPos()

#endif
