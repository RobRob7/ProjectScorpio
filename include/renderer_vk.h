#ifndef RENDERER_VK_H
#define RENDERER_VK_H

#include "i_renderer.h"

#include "image_vk.h"

#include <memory>

class VulkanMain;
class Camera;
struct RenderInputs;
struct RenderSettings;
struct FrameContext;
struct ChunkDrawList;

class ChunkMeshGPUVk;
class RayTracingWorldVk;
class RTAOPassVk;
class RTShadowPassVk;
class RayTracingWorldPassVk;

class GBufferPassVk;
class ShadowMapPassVk;
class DebugPassVk;
class SSAOPassVk;

class ChunkPassVk;
class WaterPassVk;

class HybridCompositePassVk;
class PostCompositePassVk;

class FXAAPassVk;
class FogPassVk;
class GodRayPassVk;
class PresentPassVk;

class UI;

class RendererVk final : public IRenderer
{
public:
	explicit RendererVk(VulkanMain& vk);
	~RendererVk() override;

	void init() override;
	void resize(int w, int h) override;

	void renderFrame(
		const RenderInputs& in,
		const FrameContext* pFrame,
		UI* ui
	) override;

	RenderSettings& settings() override { return *rs_; }

private:
	void createSceneAttachments();
private:
	int width_{};
	int height_{};

	VulkanMain& vk_;

	ImageVk sceneColor_;
	ImageVk sceneDepth_;

	vk::Format sceneColorFormat_{ vk::Format::eR16G16B16A16Sfloat };
	vk::Format sceneDepthFormat_{ vk::Format::eD32Sfloat };

	std::unique_ptr<RenderSettings> rs_;

	std::unique_ptr<GBufferPassVk> gbufferPass_;
	std::unique_ptr<ShadowMapPassVk> shadowMapPass_;
	std::unique_ptr<DebugPassVk> debugPass_;
	std::unique_ptr<SSAOPassVk> ssaoPass_;

	std::unique_ptr<WaterPassVk> waterPass_;
	std::unique_ptr<ChunkPassVk> chunkPass_;

	std::unique_ptr<RayTracingWorldVk> rtWorld_;
	std::unique_ptr<RTAOPassVk> rtaoPass_;
	std::unique_ptr<RTShadowPassVk> rtShadowPass_;
	std::unique_ptr<RayTracingWorldPassVk> rtWorldPass_;

	std::unique_ptr<HybridCompositePassVk> compositePassHybrid_;
	std::unique_ptr<PostCompositePassVk> compositePassPost_;

	std::unique_ptr<FogPassVk> fogPass_;
	std::unique_ptr<GodRayPassVk> godRayPass_;
	std::unique_ptr<FXAAPassVk> fxaaPass_;

	std::unique_ptr<PresentPassVk> presentPass_;
};

#endif
