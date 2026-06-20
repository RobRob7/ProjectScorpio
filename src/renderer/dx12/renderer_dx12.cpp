#include "renderer_dx12.h"

#include "chunk_draw_list.h"
//#include "chunk_mesh_gpu_vk.h"

#include "frame_context_dx12.h"

#include "utils_dx12.h"
#include "dx12_main.h"

#include "render_settings.h"
#include "render_inputs.h"
//#include "render_target_vk.h"

//#include "camera.h"
//#include "i_light.h"
//#include "i_cubemap.h"
//#include "cubemap_vk.h"
//#include "i_crosshair.h"
//#include "chunk_manager.h"
#include "ui.h"
//
//#include "ray_tracing_world_vk.h"
//#include "rtao_pass_vk.h"
//#include "rt_shadow_pass_vk.h"
//#include "ray_tracing_world_pass_vk.h"
//
//#include "gbuffer_pass_vk.h"
//#include "shadow_map_pass_vk.h"
//#include "debug_pass_vk.h"
//#include "ssao_pass_vk.h"
//#include "water_pass_vk.h"
//#include "chunk_pass_vk.h"
//#include "hybrid_composite_pass_vk.h"
//#include "post_composite_pass_vk.h"
//#include "fxaa_pass_vk.h"
//#include "fog_pass_vk.h"
//#include "god_ray_pass_vk.h"
//#include "present_pass_vk.h"

#include <glm/glm.hpp>

//--- PUBLIC ---//
RendererDX12::RendererDX12(DX12Main& dx)
	: dx_(dx)
{
} // end of constructor

RendererDX12::~RendererDX12() = default;

void RendererDX12::init()
{
	if (!rs_) 
	{
		rs_ = std::make_unique<RenderSettings>();
	}

	//if (vk_.supportsRayTracing())
	//{
	//	if (!rtWorld_)
	//	{
	//		rtWorld_ = std::make_unique<RayTracingWorldVk>(vk_);
	//	}

	//	if (!rtaoPass_)
	//	{
	//		rtaoPass_ = std::make_unique<RTAOPassVk>(
	//			vk_,
	//			*rs_,
	//			rtWorld_->getTLAS()
	//		);
	//		rtaoPass_->init();
	//	}
	//	if (!rtShadowPass_)
	//	{
	//		rtShadowPass_ = std::make_unique<RTShadowPassVk>(
	//			vk_,
	//			*rs_,
	//			rtWorld_->getTLAS()
	//		);
	//		rtShadowPass_->init();
	//	}

	//	if (!rtWorldPass_)
	//	{
	//		rtWorldPass_ = std::make_unique<RayTracingWorldPassVk>(
	//			vk_,
	//			*rs_,
	//			rtWorld_->getTLAS(),
	//			rtWorld_->getPackedRTOpaqueInfoBuffer(),
	//			rtWorld_->getPackedRTOpaqueInfoBufferSize(),
	//			rtWorld_->getPackedRTWaterInfoBuffer(),
	//			rtWorld_->getPackedRTWaterInfoBufferSize()
	//		);
	//		rtWorldPass_->init();
	//	}
	//}
	//else
	//{
	//	rs_->useRT = false;
	//	rs_->useRTAO = false;
	//	rs_->useRTShadow = false;
	//}

	//if (!gbufferPass_)
	//{
	//	gbufferPass_ = std::make_unique<GBufferPassVk>(vk_);
	//}
	//if (!shadowMapPass_)
	//{
	//	shadowMapPass_ = std::make_unique<ShadowMapPassVk>(vk_);
	//}
	//if (!debugPass_)
	//{
	//	debugPass_ = std::make_unique<DebugPassVk>(
	//		vk_
	//		//gbufferPass_->getNormalImage(),
	//		//gbufferPass_->getDepthImage(),
	//		//shadowMapPass_->getDepthImage(),
	//		//shadowMapPass_->getDepthImage()
	//		////rtWorldPass_->getOutDepthImage()
	//	);
	//}
	//if (!ssaoPass_)
	//{
	//	ssaoPass_ = std::make_unique<SSAOPassVk>(vk_, *rs_);
	//}

	//if (!waterPass_)
	//{
	//	waterPass_ = std::make_unique<WaterPassVk>(vk_, *rs_);
	//}
	//if (!chunkPass_)
	//{
	//	chunkPass_ = std::make_unique<ChunkPassVk>(vk_, *rs_);
	//}

	//if (!compositePassHybrid_)
	//{
	//	compositePassHybrid_ = std::make_unique<HybridCompositePassVk>(vk_);
	//}

	//if (!compositePassPost_)
	//{
	//	compositePassPost_ = std::make_unique<PostCompositePassVk>(vk_);
	//}

	//if (!fogPass_)
	//{
	//	fogPass_ = std::make_unique<FogPassVk>(vk_, *rs_);
	//}
	//if (!godRayPass_)
	//{
	//	godRayPass_ = std::make_unique<GodRayPassVk>(vk_, *rs_);
	//}
	//if (!fxaaPass_)
	//{
	//	fxaaPass_ = std::make_unique<FXAAPassVk>(vk_);
	//}

	//if (!presentPass_)
	//{
	//	presentPass_ = std::make_unique<PresentPassVk>(vk_);
	//}

	//gbufferPass_->init();
	//shadowMapPass_->init();
	//debugPass_->init();
	//ssaoPass_->init();

	//waterPass_->init();
	//chunkPass_->init(
	//	{sceneColorFormat_, sceneDepthFormat_},
	//	{gbufferPass_->getNormalImage().format(), gbufferPass_->getDepthImage().format()},
	//	{vk::Format::eUndefined, shadowMapPass_->getDepthImage().format()}
	//);

	//compositePassHybrid_->init();
	//compositePassPost_->init();

	//fogPass_->init();
	//godRayPass_->init();
	//fxaaPass_->init();
	//presentPass_->init();
} // end of init()

void RendererDX12::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (w == width_ && h == height_) return;

	width_ = w;
	height_ = h;

	//if (rtaoPass_)		rtaoPass_->resize();
	//if (rtShadowPass_)	rtShadowPass_->resize();
	//if (rtWorldPass_)	rtWorldPass_->resize();

	//if (gbufferPass_)	gbufferPass_->resize();
	//if (debugPass_)		debugPass_->resize();
	//if (ssaoPass_)		ssaoPass_->resize();

	//if (waterPass_)		waterPass_->resize();
	//if (chunkPass_)		chunkPass_->resize();

	//if (compositePassHybrid_)	compositePassHybrid_->resize();
	//if (compositePassPost_)	compositePassPost_->resize();

	//if (fogPass_)		fogPass_->resize();
	//if (godRayPass_)	godRayPass_->resize();
	//if (fxaaPass_)		fxaaPass_->resize();

	//if (presentPass_)	presentPass_->resize();

	createSceneAttachments();
} // end of resize()

void RendererDX12::renderFrame(
	const RenderInputs& in,
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	UI* ui
)
{
	if (!frameDX12)
	{
		return;
	}

	ID3D12GraphicsCommandList* cmd = frameDX12->cmd;

	D3D12_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(frameDX12->width);
	viewport.Height = static_cast<float>(frameDX12->height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissor{};
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = static_cast<LONG>(frameDX12->width);
	scissor.bottom = static_cast<LONG>(frameDX12->height);

	cmd->RSSetViewports(1, &viewport);
	cmd->RSSetScissorRects(1, &scissor);

	cmd->OMSetRenderTargets(
		1,
		&frameDX12->colorRTV,
		FALSE,
		&frameDX12->depthDSV
	);

	const float clearColor[4] =
	{
		0.05f, 0.07f, 0.10f, 1.0f
	};

	cmd->ClearRenderTargetView(
		frameDX12->colorRTV,
		clearColor,
		0,
		nullptr
	);

	cmd->ClearDepthStencilView(
		frameDX12->depthDSV,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr
	);

	if (ui)
	{
		cmd->OMSetRenderTargets(
			1,
			&frameDX12->colorRTV,
			FALSE,
			nullptr
		);

		ui->renderDX12(cmd);
	}
} // end of renderFrame()


//--- PRIVATE ---//
void RendererDX12::createSceneAttachments()
{
	//// SCENE COLOR
	//sceneColor_.createImage(
	//	width_,
	//	height_,
	//	1,
	//	false,
	//	vk::SampleCountFlagBits::e1,
	//	sceneColorFormat_,
	//	vk::ImageTiling::eOptimal,
	//	vk::ImageUsageFlagBits::eColorAttachment |
	//	vk::ImageUsageFlagBits::eSampled |
	//	vk::ImageUsageFlagBits::eTransferDst,
	//	vk::MemoryPropertyFlagBits::eDeviceLocal
	//);

	//sceneColor_.createImageView(
	//	sceneColorFormat_,
	//	vk::ImageAspectFlagBits::eColor,
	//	vk::ImageViewType::e2D,
	//	1
	//);

	//sceneColor_.createSampler(
	//	vk::Filter::eNearest,
	//	vk::Filter::eNearest,
	//	vk::SamplerMipmapMode::eNearest,
	//	vk::SamplerAddressMode::eClampToEdge,
	//	vk::False
	//);

	//sceneColor_.setDebugName("RendererDX12-SceneColor");


	//// SCENE DEPTH
	//sceneDepth_.createImage(
	//	width_,
	//	height_,
	//	1,
	//	false,
	//	vk::SampleCountFlagBits::e1,
	//	sceneDepthFormat_,
	//	vk::ImageTiling::eOptimal,
	//	vk::ImageUsageFlagBits::eDepthStencilAttachment |
	//	vk::ImageUsageFlagBits::eSampled,
	//	vk::MemoryPropertyFlagBits::eDeviceLocal
	//);

	//sceneDepth_.createImageView(
	//	sceneDepthFormat_,
	//	vk::ImageAspectFlagBits::eDepth,
	//	vk::ImageViewType::e2D,
	//	1
	//);

	//sceneDepth_.createSampler(
	//	vk::Filter::eNearest,
	//	vk::Filter::eNearest,
	//	vk::SamplerMipmapMode::eNearest,
	//	vk::SamplerAddressMode::eClampToEdge,
	//	vk::False
	//);

	//sceneDepth_.setDebugName("RendererDX12-SceneDepth");
} // end of createSceneAttachments()
