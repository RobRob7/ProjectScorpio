Texture2D<float4> FogTex : register(t1);
Texture2D<float4> GodRayTex : register(t2);
Texture2D<float3> SceneColorTex : register(t3);
RWTexture2D<float4> PostCompositeImage : register(u4);

SamplerState LinearClampSampler : register(s0);

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pixel = int2(dispatchThreadID.xy);
    
    uint outWidth;
    uint outHeight;
    PostCompositeImage.GetDimensions(outWidth, outHeight);
    
    int2 outSize = int2(outWidth, outHeight);

    if (pixel.x >= outSize.x || pixel.y >= outSize.y)
        return;

    float2 uv = (float2(pixel) + 0.5) / float2(outSize);
    
    float4 fogColor = FogTex.Sample(LinearClampSampler, uv);
    float4 godRayColor = GodRayTex.Sample(LinearClampSampler, uv);
    float3 sceneColor = SceneColorTex.Sample(LinearClampSampler, uv).rgb;

	float3 finalColor = sceneColor * fogColor.a + fogColor.rgb;
	//finalColor += godRayColor.rgb;

	PostCompositeImage[pixel] = float4(finalColor.rgb, 1.0);
}