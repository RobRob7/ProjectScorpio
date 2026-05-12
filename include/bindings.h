#ifndef BINDINGS_H
#define BINDINGS_H

#include <cstdint>

template<typename E>
constexpr uint32_t TO_API_FORM(E e)
{
    return static_cast<uint32_t>(e);
}

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
};
enum class RTWaterClosestHitBinding : uint32_t
{
    TLAS = 0,
    WaterInfo = 1,
    UBO = 2,
    DudvTex = 3,
    NormalTex = 4,
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
    ForwardColorTex = 1,
    ForwardDepthTex = 2,
    ShadowMapTex = 3,
    OutColorTex = 4
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
    GNormalTex = 1,
    GDepthTex = 2,
    NoiseTex = 3,
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
    FogColorTex = 1,
    SceneColorTex = 2,
};

#endif
