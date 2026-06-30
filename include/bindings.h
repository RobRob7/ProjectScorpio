#ifndef BINDINGS_H
#define BINDINGS_H

#include <cstdint>

template<typename E>
constexpr uint32_t TO_API_FORM(E e)
{
    return static_cast<uint32_t>(e);
}

enum class RTAORayGenBinding : uint32_t
{
    OutColorImage = 0,
    TLAS = 1,
    UBO = 2,
    NormalTex = 3,
    DepthTex = 4,
};

enum class RTShadowRayGenBinding : uint32_t
{
    OutColorImage = 0,
    TLAS = 1,
    UBO = 2,
    NormalTex = 3,
    DepthTex = 4,
};

enum class RTChunkRayGenBinding : uint32_t
{
    OutColorImage = 0,
    OutDepthImage = 1,
    TLAS = 2,
    UBO = 3,
};
enum class RTChunkMissBinding : uint32_t
{
    UBO = 0,
    NightSkyboxTex = 1,
    DaySkyboxTex = 2,
};
enum class RTOpaqueClosestHitBinding : uint32_t
{
    TLAS = 0,
    ChunkInfo = 1,
    UBO = 2,
    AtlasTex = 3,
    RTAOTex = 4,
    RTShadowTex = 5,
};
enum class RTWaterClosestHitBinding : uint32_t
{
    TLAS = 0,
    WaterInfo = 1,
    UBO = 2,
    DudvTex = 3,
    NormalTex = 4,
    RTShadowTex = 5,
};

enum class ChunkBinding : uint32_t
{
    UBO = 0,
    AtlasTex = 1,
    SSAOTex = 2,
    ShadowTex = 3,
};

enum class WaterBinding : uint32_t
{
    UBO = 0,
    ReflColorTex = 1,
    RefrColorTex = 2,
    RefrDepthTex = 3,
    DudvTex = 4,
    NormalTex = 5,
    ShadowTex = 6,
};

enum class DebugBinding : uint32_t
{
    UBO = 0,
    GNormalTex = 1,
    GDepthTex = 2,
    ShadowMapTex = 3,
    RTDepthTex = 4,
};

enum class ShadowMapPassBinding : uint32_t
{
    UBO = 0,
};

enum class FogPassBinding : uint32_t
{
    UBO = 0,
    ForwardDepthTex = 1,
    OutColorTex = 2
};

enum class GodRayPassBinding : uint32_t
{
    UBO = 0,
    ForwardDepthTex = 1,
    ShadowMapTex = 2,
    OutColorTex = 3
};

enum class GbufferBinding : uint32_t
{
    UBO = 0,
    ForwardColorTex = 1,
    ForwardDepthTex = 2,
};

enum class CubemapBinding : uint32_t
{
    UBO = 0,
    NightSkyboxTex = 1,
    DaySkyboxTex = 2,
};

enum class FXAAPassBinding : uint32_t
{
    UBO = 0,
    ForwardColorTex = 1,
};

enum class LightBinding : uint32_t
{
    UBO = 0,
};

enum class SSAORawBinding : uint32_t
{
    UBO = 0,
    SamplesUBO = 1,
    GNormalTex = 2,
    GDepthTex = 3,
    NoiseTex = 4,
};

enum class SSAOBlurBinding : uint32_t
{
    UBO = 0,
    SSAORawTex = 1,
};


enum class PresentPassBinding : uint32_t
{
    UBO = 0,
    ForwardColorTex = 1,
    PostProcessTex
};

enum class HybridCompositePassBinding : uint32_t
{
    UBO = 0,
    RastColorTex = 1,
    RastDepthTex = 2,
    RTColorTex = 3,
    RTDepthTex = 4,
};

enum class PostCompositePassBinding : uint32_t
{
    UBO = 0,
    FogTex = 1,
    GodRayTex = 2,
    SceneColorTex = 3,
    PostOutColorTex = 4,
};

#endif
