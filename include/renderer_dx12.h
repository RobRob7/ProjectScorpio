#ifndef RENDERER_DX12_H
#define RENDERER_DX12_H

#include "i_renderer.h"

#include "image_dx12.h"

#include <memory>

class DX12Main;
class Camera;
struct RenderInputs;
struct RenderSettings;
struct FrameContextDX12;
struct ChunkDrawList;

//class ChunkMeshGPUVk;
//class RayTracingWorldVk;
//class RTAOPassVk;
//class RTShadowPassVk;
//class RayTracingWorldPassVk;
//
//class GBufferPassVk;
//class ShadowMapPassVk;
//class DebugPassVk;
//class SSAOPassVk;
//
class ChunkPassDX12;
//class WaterPassVk;
//
//class HybridCompositePassVk;
//class PostCompositePassVk;
//
//class FXAAPassVk;
//class FogPassVk;
//class GodRayPassVk;
class PresentPassDX12;

class UI;

class RendererDX12 final : public IRenderer
{
public:
	explicit RendererDX12(DX12Main& dx);
	~RendererDX12() override;

	void init() override;
	void resize(int w, int h) override;

	void renderFrame(
		const RenderInputs& in,
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		UI* ui
	) override;

	RenderSettings& settings() override { return *rs_; }

private:
	void createSceneAttachments();
private:
	int width_{};
	int height_{};

	DX12Main* dx_{ nullptr };

	ImageDX12 sceneColor_;
	ImageDX12 sceneDepth_;

	DXGI_FORMAT sceneColorFormat_{ DXGI_FORMAT_R16G16B16A16_FLOAT };
	DXGI_FORMAT sceneDepthFormat_{ DXGI_FORMAT_D32_FLOAT };

	std::unique_ptr<RenderSettings> rs_;

	//std::unique_ptr<GBufferPassVk> gbufferPass_;
	//std::unique_ptr<ShadowMapPassVk> shadowMapPass_;
	//std::unique_ptr<DebugPassVk> debugPass_;
	//std::unique_ptr<SSAOPassVk> ssaoPass_;

	//std::unique_ptr<WaterPassVk> waterPass_;
	std::unique_ptr<ChunkPassDX12> chunkPass_;

	//std::unique_ptr<RayTracingWorldVk> rtWorld_;
	//std::unique_ptr<RTAOPassVk> rtaoPass_;
	//std::unique_ptr<RTShadowPassVk> rtShadowPass_;
	//std::unique_ptr<RayTracingWorldPassVk> rtWorldPass_;

	//std::unique_ptr<HybridCompositePassVk> compositePassHybrid_;
	//std::unique_ptr<PostCompositePassVk> compositePassPost_;

	//std::unique_ptr<FogPassVk> fogPass_;
	//std::unique_ptr<GodRayPassVk> godRayPass_;
	//std::unique_ptr<FXAAPassVk> fxaaPass_;

	std::unique_ptr<PresentPassDX12> presentPass_;
};

#endif
