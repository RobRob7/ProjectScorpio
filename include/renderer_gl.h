#ifndef RENDERER_GL_H
#define RENDERER_GL_H

#include "i_renderer.h"
#include "render_settings.h"

#include <cstdint>
#include <memory>

class ChunkPassGL;

class ChunkManager;
class Camera;
class Light;
class CubeMap;
class Crosshair;

class GBufferPass;
class ShadowMapPassGL;
class DebugPass;
class SSAOPass;
class FXAAPass;
class PresentPass;
class WaterPass;
class FogPassGL;
class GodRayPassGL;
class PostCompositePassGL;

struct RenderInputs;
struct FrameContext;
class UI;

class RendererGL : public IRenderer
{
public:
	RendererGL();
	~RendererGL() override;

	void init() override;
	void resize(int w, int h) override;
	void renderFrame(
		const RenderInputs& in,
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		UI* ui
	) override;

	RenderSettings& settings() override;

private:
	void destroyGL();
	void resizeForwardTargets();
private:
	int width_{};
	int height_{};

	std::unique_ptr<RenderSettings> rs_;

	// passes
	std::unique_ptr<GBufferPass> gbuffer_;
	std::unique_ptr<ShadowMapPassGL> shadowMapPass_;
	std::unique_ptr<DebugPass> debugPass_;
	std::unique_ptr<SSAOPass> ssaoPass_;

	std::unique_ptr<WaterPass> waterPass_;
	std::unique_ptr<ChunkPassGL> chunkPass_;

	std::unique_ptr<PostCompositePassGL> compositePassPost_;

	std::unique_ptr<FogPassGL> fogPass_;
	std::unique_ptr<GodRayPassGL> godRayPass_;
	std::unique_ptr<FXAAPass> fxaaPass_;

	std::unique_ptr<PresentPass> presentPass_;

	uint32_t forwardFBO_{};
	uint32_t forwardColorTex_{};
	uint32_t forwardDepthTex_{};
};

#endif
