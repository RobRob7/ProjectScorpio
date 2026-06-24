#include "renderer_dx12.h"

#include "chunk_draw_list.h"
#include "chunk_mesh_gpu_dx12.h"

#include "frame_context_dx12.h"

#include "utils_dx12.h"
#include "dx12_main.h"

#include "render_settings.h"
#include "render_inputs.h"
#include "render_target_dx12.h"

#include "camera.h"
#include "i_light.h"
#include "i_cubemap.h"
#include "i_crosshair.h"
#include "chunk_manager.h"
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
#include "chunk_pass_dx12.h"
//#include "hybrid_composite_pass_vk.h"
//#include "post_composite_pass_vk.h"
//#include "fxaa_pass_vk.h"
//#include "fog_pass_vk.h"
//#include "god_ray_pass_vk.h"
#include "present_pass_dx12.h"

#include <glm/glm.hpp>

//--- PUBLIC ---//
RendererDX12::RendererDX12(DX12Main& dx)
	: dx_(&dx),
	sceneColor_(dx),
	sceneDepth_(dx)
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
	if (!chunkPass_)
	{
		chunkPass_ = std::make_unique<ChunkPassDX12>(*dx_, *rs_);
	}

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

	if (!presentPass_)
	{
		presentPass_ = std::make_unique<PresentPassDX12>(*dx_);
	}

	//gbufferPass_->init();
	//shadowMapPass_->init();
	//debugPass_->init();
	//ssaoPass_->init();

	//waterPass_->init();
	chunkPass_->init(
		{sceneColorFormat_, sceneDepthFormat_},
		{ sceneColorFormat_, sceneDepthFormat_ },
		{ sceneColorFormat_, sceneDepthFormat_ }
		//{gbufferPass_->getNormalImage().format(), gbufferPass_->getDepthImage().format()},
		//{vk::Format::eUndefined, shadowMapPass_->getDepthImage().format()}
	);

	//compositePassHybrid_->init();
	//compositePassPost_->init();

	//fogPass_->init();
	//godRayPass_->init();
	//fxaaPass_->init();
	presentPass_->init();
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
	if (chunkPass_)		chunkPass_->resize();

	//if (compositePassHybrid_)	compositePassHybrid_->resize();
	//if (compositePassPost_)	compositePassPost_->resize();

	//if (fogPass_)		fogPass_->resize();
	//if (godRayPass_)	godRayPass_->resize();
	//if (fxaaPass_)		fxaaPass_->resize();

	if (presentPass_)	presentPass_->resize();

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

	FrameContextDX12& frame = *const_cast<FrameContextDX12*>(frameDX12);

	const glm::mat4 view = in.camera->getViewMatrix();
	const float aspect = (height_ > 0)
		? (static_cast<float>(width_) / static_cast<float>(height_))
		: 1.0f;
	glm::mat4 proj = in.camera->getProjectionMatrix(aspect);

	ID3D12GraphicsCommandList* cmd = frame.cmd;

	// update light/sun
	in.light->updateLight(
		in.time,
		in.camera->getCameraPosition(),
		rs_->sunPaused
	);

	// update world state
	in.world->updateDynamic(in.camera->getCameraPosition(), nullptr, &frame);
	//if (vk_.supportsRayTracing() && rs_->useRT)
	//{
	//	in.world->buildRTDrawList(view, proj);
	//}
	//else
	//{
	//	in.world->buildWaterDrawList(view, proj);
	//}

	// --------------- FORWARD RENDER --------------- //
	sceneColor_.transitionToRenderTarget(cmd);
	sceneDepth_.transitionToDepthWrite(cmd);

	D3D12_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(frame.width);
	viewport.Height = static_cast<float>(frame.height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissor{};
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = static_cast<LONG>(frame.width);
	scissor.bottom = static_cast<LONG>(frame.height);

	cmd->RSSetViewports(1, &viewport);
	cmd->RSSetScissorRects(1, &scissor);

	D3D12_CPU_DESCRIPTOR_HANDLE colorRTV = sceneColor_.rtvCPU();
	D3D12_CPU_DESCRIPTOR_HANDLE depthDSV = sceneDepth_.dsvCPU();

	cmd->OMSetRenderTargets(
		1,
		&colorRTV,
		FALSE,
		&depthDSV
	);

	const float clearColor[4] =
	{
		0.0f, 0.0f, 0.0f, 1.0f
	};

	cmd->ClearRenderTargetView(
		colorRTV,
		clearColor,
		0,
		nullptr
	);

	cmd->ClearDepthStencilView(
		depthDSV,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr
	);

	if (chunkPass_ && !rs_->useRT)
	{
		chunkPass_->setInput(
			sceneColor_,
			sceneColor_
		);

		chunkPass_->renderOpaque(
			RenderTargetDX12::Default,
			in,
			frame,
			view,
			proj,
			glm::mat4(1.0)
			//shadowMapPass_->getLightSpaceMatrix()
		);
	}

	{
		in.skybox->render(
			nullptr,
			&frame,
			view,
			proj,
			in.light->getDirection()
		);
	}

	{
		in.light->render(
			nullptr,
			&frame,
			view,
			proj
		);
	}

	// --------------- END FORWARD RENDER --------------- //

	// scene color + depth transition to shader read
	sceneColor_.transitionToShaderRead(cmd);
	sceneDepth_.transitionToDepthWrite(cmd);

	// ----------------- POST-PROCESSING ----------------- //
	ImageDX12* sceneColor = nullptr;
	ImageDX12* sceneDepth = nullptr;
	ImageDX12* currentColor = &sceneColor_;
	//if (vk_.supportsRayTracing() && rs_->useRT)
	//{
	//	sceneColor = &compositePassHybrid_->getOutColorImage();
	//	sceneDepth = &compositePassHybrid_->getOutDepthImage();
	//}
	//else
	//{
	//	sceneColor = &sceneColor_;
	//	sceneDepth = &sceneDepth_;
	//}
	// --------------- END POST-PROCESSING --------------- //

	// swap swapchain color image to color attachment
	frame.transitionColorImageToAttachment(cmd);

	// ----------------- PRESENT PASS ----------------- //
	if (presentPass_)
	{
		presentPass_->setInput(*currentColor);
		presentPass_->render(frame);
	}
	// --------------- END PRESENT PASS --------------- //


	// ----------------- UI ELEMENTS ----------------- //
	// CROSSHAIR RENDER
	if (in.crosshair)
	{
		in.crosshair->render(nullptr, &frame);
	}

	// UI RENDER
	if (ui)
	{
		ui->renderDX12(frame);
	}
	// --------------- END UI ELEMENTS --------------- //
} // end of renderFrame()


//--- PRIVATE ---//
void RendererDX12::createSceneAttachments()
{
	// SCENE COLOR
	D3D12_CLEAR_VALUE colorClear{
		.Format = sceneColorFormat_,
		.Color = {0.0f, 0.0f, 0.0f, 1.0f}
	};

	sceneColor_.createImage(
		static_cast<uint32_t>(width_),
		static_cast<uint32_t>(height_),
		1,
		false,
		sceneColorFormat_,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&colorClear
	);
	sceneColor_.createRTV();
	sceneColor_.setDebugName(L"RendererDX12-SceneColor");

	// SCENE DEPTH
	D3D12_CLEAR_VALUE depthClear{
		.Format = sceneDepthFormat_,
		.DepthStencil = 
		{
			.Depth = 1.0f,
			.Stencil = 0
		}
	};

	sceneDepth_.createImage(
		static_cast<uint32_t>(width_),
		static_cast<uint32_t>(height_),
		1,
		false,
		sceneDepthFormat_,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClear
	);
	sceneDepth_.createDSV();
	sceneDepth_.setDebugName(L"RendererDX12-SceneDepth");
} // end of createSceneAttachments()
