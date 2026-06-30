Texture2D<float4> SrcMip : register(t0);
RWTexture2D<float4> DstMip : register(u1);

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint dstWidth;
    uint dstHeight;
    DstMip.GetDimensions(dstWidth, dstHeight);
    
    if (dispatchThreadID.x >= dstWidth || dispatchThreadID.y >= dstHeight)
    {
        return;
    }
    
    uint srcWidth, srcHeight;
    SrcMip.GetDimensions(srcWidth, srcHeight);

    uint2 src0 = dispatchThreadID.xy * 2;
    uint2 src1 = min(src0 + uint2(1, 0), uint2(srcWidth - 1, srcHeight - 1));
    uint2 src2 = min(src0 + uint2(0, 1), uint2(srcWidth - 1, srcHeight - 1));
    uint2 src3 = min(src0 + uint2(1, 1), uint2(srcWidth - 1, srcHeight - 1));

    float4 c0 = SrcMip[min(src0, uint2(srcWidth - 1, srcHeight - 1))];
    float4 c1 = SrcMip[src1];
    float4 c2 = SrcMip[src2];
    float4 c3 = SrcMip[src3];

    DstMip[dispatchThreadID.xy] = (c0 + c1 + c2 + c3) * 0.25;
}