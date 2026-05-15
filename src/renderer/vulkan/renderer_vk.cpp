#include "renderer_vk.h"

#include "chunk_draw_list.h"
#include "chunk_mesh_gpu_vk.h"

#include "frame_context_vk.h"

#include "utils_vk.h"
#include "vulkan_main.h"

#include "render_settings.h"
#include "render_inputs.h"
#include "render_target_vk.h"

#include "camera.h"
#include "i_light.h"
#include "i_cubemap.h"
#include "cubemap_vk.h"
#include "i_crosshair.h"
#include "chunk_manager.h"
#include "ui.h"

#include "ray_tracing_world_pass_vk.h"

#include "gbuffer_pass_vk.h"
#include "shadow_map_pass_vk.h"
#include "debug_pass_vk.h"
#include "ssao_pass_vk.h"
#include "water_pass_vk.h"
#include "chunk_pass_vk.h"
#include "hybrid_composite_pass_vk.h"
#include "post_composite_pass_vk.h"
#include "fxaa_pass_vk.h"
#include "fog_pass_vk.h"
#include "present_pass_vk.h"

#include <glm/glm.hpp>

//--- PUBLIC ---//
RendererVk::RendererVk(VulkanMain& vk)
	: vk_(vk),
	sceneColor_(vk),
	sceneDepth_(vk)
{
} // end of constructor

RendererVk::~RendererVk() = default;

void RendererVk::init()
{
	if (!renderSettings_) 
	{
		renderSettings_ = std::make_unique<RenderSettings>();
	}

	if (vk_.supportsRayTracing() && !rtWorldPass_)
	{
		rtWorldPass_ = std::make_unique<RayTracingWorldPassVk>(vk_);
	}

	if (!gbufferPass_)
	{
		gbufferPass_ = std::make_unique<GBufferPassVk>(vk_);
	}
	if (!shadowMapPass_)
	{
		shadowMapPass_ = std::make_unique<ShadowMapPassVk>(vk_);
	}
	if (!debugPass_)
	{
		debugPass_ = std::make_unique<DebugPassVk>(
			vk_,
			gbufferPass_->getNormalImage(),
			gbufferPass_->getDepthImage(),
			shadowMapPass_->getDepthImage(),
			shadowMapPass_->getDepthImage()
			//rtWorldPass_->getOutDepthImage()
		);
	}
	if (!ssaoPass_)
	{
		ssaoPass_ = std::make_unique<SSAOPassVk>(
			vk_,
			gbufferPass_->getNormalImage(),
			gbufferPass_->getDepthImage()
		);
	}

	if (!waterPass_)
	{
		waterPass_ = std::make_unique<WaterPassVk>(
			vk_,
			shadowMapPass_->getDepthImage()
		);
	}
	if (!chunkPass_)
	{
		chunkPass_ = std::make_unique<ChunkPassVk>(
			vk_,
			*renderSettings_,
			ssaoPass_->ssaoBlurImage(),
			shadowMapPass_->getDepthImage()
		);
	}

	if (!compositePassHybrid_)
	{
		compositePassHybrid_ = std::make_unique<HybridCompositePassVk>(vk_);
	}

	if (!compositePassPost_)
	{
		compositePassPost_ = std::make_unique<PostCompositePassVk>(vk_);
	}

	if (!fogPass_)
	{
		fogPass_ = std::make_unique<FogPassVk>(vk_);
	}
	if (!fxaaPass_)
	{
		fxaaPass_ = std::make_unique<FXAAPassVk>(vk_);
	}

	if (!presentPass_)
	{
		presentPass_ = std::make_unique<PresentPassVk>(vk_);
	}

	if (rtWorldPass_)
	{
		rtWorldPass_->init();
	}
	else
	{
		renderSettings_->useRT = false;
	}

	gbufferPass_->init();
	shadowMapPass_->init();
	debugPass_->init();
	ssaoPass_->init();

	waterPass_->init();
	chunkPass_->init(
		{sceneColorFormat_, sceneDepthFormat_},
		{gbufferPass_->getNormalImage().format(), gbufferPass_->getDepthImage().format()},
		{vk::Format::eUndefined, shadowMapPass_->getDepthImage().format()}
	);

	compositePassHybrid_->init();
	compositePassPost_->init();

	fogPass_->init();
	fxaaPass_->init();
	presentPass_->init();
} // end of init()

void RendererVk::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (w == width_ && h == height_) return;

	width_ = w;
	height_ = h;

	if (rtWorldPass_)	rtWorldPass_->resize();

	if (gbufferPass_)	gbufferPass_->resize();
	if (debugPass_)		debugPass_->resize();
	if (ssaoPass_)		ssaoPass_->resize();

	if (waterPass_)		waterPass_->resize();
	if (chunkPass_)		chunkPass_->resize();

	if (compositePassHybrid_)	compositePassHybrid_->resize();
	if (compositePassPost_)	compositePassPost_->resize();

	if (fogPass_)		fogPass_->resize();
	if (fxaaPass_)		fxaaPass_->resize();

	if (presentPass_)	presentPass_->resize();

	createSceneAttachments();
} // end of resize()

void RendererVk::renderFrame(
	const RenderInputs& in,
	const FrameContext* pFrame,
	UI* ui
)
{
	FrameContext& frame = *const_cast<FrameContext*>(pFrame);

	if (frame.extent.width != width_ || frame.extent.height != height_)
	{
		resize(frame.extent.width, frame.extent.height);
	}

	const glm::mat4 view = in.camera->getViewMatrix();
	const float aspect = (height_ > 0)
		? (static_cast<float>(width_) / static_cast<float>(height_))
		: 1.0f;
	glm::mat4 proj = in.camera->getProjectionMatrixVk(aspect);

	vk::CommandBuffer cmd = frame.cmd;

	// update light/sun
	in.light->updateLight(
		in.time, 
		in.camera->getCameraPosition(),
		renderSettings_->sunPaused
	);

	// update world state
	in.world->updateDynamic(in.camera->getCameraPosition(), &frame);
	if (renderSettings_->useRT)
	{
		in.world->buildRTDrawList(view, proj);
	}

	// ----------------- PASSES ----------------- //
	// RT upload
	if (vk_.supportsRayTracing() && renderSettings_->useRT && rtWorldPass_)
	{
		rtWorldPass_->upload(
			cmd,
			in.world->getRTDrawList(),
			view,
			proj,
			frame.frameIndex
		);
	}

	// gbuffer pass
	if (!renderSettings_->useRT && gbufferPass_)
	{
		gbufferPass_->render(
			*chunkPass_, 
			in, 
			frame, 
			view, 
			proj
		);
	}

	// shadow map pass
	if ((!renderSettings_->useRT && shadowMapPass_) || renderSettings_->useFog)
	{
		shadowMapPass_->render(
			*chunkPass_,
			in,
			frame
		);
	}

	// ssao pass
	if (!renderSettings_->useRT && renderSettings_->useSSAO)
	{
		ssaoPass_->renderOffscreen(
			frame, 
			proj
		);
	}

	// water refl + refr pass
	if (!renderSettings_->useRT && waterPass_)
	{
		waterPass_->renderOffscreen(
			*renderSettings_,
			frame,
			proj,
			*chunkPass_,
			in,
			shadowMapPass_->getLightSpaceMatrix()
		);
	}

	// debug pass
	if (renderSettings_->debugMode != DebugMode::None)
	{
		debugPass_->render(
			frame,
			in.camera->getNearPlane(),
			in.camera->getFarPlane(),
			static_cast<int>(renderSettings_->debugMode)
		);

		if (ui)
		{
			ui->renderVk(frame);
		}

		// present
		vk_.setSwapChainLayout(frame.imageIndex, vk::ImageLayout::ePresentSrcKHR);
		return;
	}
	// --------------- END PASSES --------------- //


	// ----------------- FORWARD RENDER ----------------- //
	cmd.beginDebugUtilsLabelEXT({ "RendererVk-ForwardRaster::cmd" });

	// scene color + depth transition to attachment
	sceneColor_.transitionToColorAttachment(cmd);
	sceneDepth_.transitionToDepthAttachment(cmd);

	vk::ClearValue clear{ {0.0f, 0.0f, 0.0f, 1.0f} };

	vk::RenderingAttachmentInfo colorAttach{};
	colorAttach.imageView = sceneColor_.view();
	colorAttach.imageLayout = sceneColor_.layout();
	colorAttach.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttach.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttach.clearValue = clear;

	vk::RenderingAttachmentInfo depthAttach{};
	depthAttach.imageView = sceneDepth_.view();
	depthAttach.imageLayout = sceneDepth_.layout();
	depthAttach.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttach.storeOp = vk::AttachmentStoreOp::eStore;
	depthAttach.clearValue.depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = frame.extent;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttach;
	renderingInfo.pDepthAttachment = &depthAttach;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(frame.extent.width);
		viewport.height = static_cast<float>(frame.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = frame.extent;
		cmd.setScissor(0, 1, &scissor);

		if (chunkPass_ && !renderSettings_->useRT)
		{
			chunkPass_->renderOpaque(
				RenderTargetVk::Default,
				in,
				frame,
				view,
				proj,
				shadowMapPass_->getLightSpaceMatrix()
			);
		}

		if (waterPass_ && !renderSettings_->useRT)
		{
			waterPass_->renderWater(
				frame,
				*renderSettings_,
				in,
				view,
				proj,
				shadowMapPass_->getLightSpaceMatrix(),
				width_,
				height_
			);
		}

		if (in.skybox) 
		{
			in.skybox->render(
				&frame,
				view,
				proj,
				in.light->getDirection(),
				in.time
			);
		}

		if (in.light)
		{
			in.light->render(
				&frame,
				view,
				proj
			);
		}
	}
	cmd.endRendering();

	cmd.endDebugUtilsLabelEXT();

	// RT render
	if (vk_.supportsRayTracing() && renderSettings_->useRT && rtWorldPass_)
	{
		CubemapVk* skybox = dynamic_cast<CubemapVk*>(in.skybox);
		rtWorldPass_->setSkybox(
			frame.frameIndex,
			skybox->getNightTexture(),
			skybox->getDayTexture()
		);
		rtWorldPass_->render(
			in,
			frame,
			view,
			proj,
			in.light->getDirection()
		);
	}
	// --------------- END FORWARD RENDER --------------- //

	// scene color + depth transition to shader read
	sceneColor_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eColor);
	sceneDepth_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);

	if (renderSettings_->useRT)
	{
		// RT color + depth transition to shader read
		rtWorldPass_->getOutColorImage().transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eColor);
		rtWorldPass_->getOutDepthImage().transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eColor);

	}

	// ----------------- HYBRID COMPOSITE PASS ----------------- //
	if (vk_.supportsRayTracing() && renderSettings_->useRT)
	{
		compositePassHybrid_->setInput(
			{ sceneColor_, sceneDepth_ },
			{ rtWorldPass_->getOutColorImage(), rtWorldPass_->getOutDepthImage() }
		);
		compositePassHybrid_->render(
			frame,
			in.camera->getNearPlane(),
			in.camera->getFarPlane()
		);
	}
	// --------------- END HYBRID COMPOSITE PASS --------------- //


	// ----------------- POST-PROCESSING ----------------- //
	ImageVk* finalSceneColor = nullptr;
	ImageVk* finalSceneDepth = nullptr;
	ImageVk* postBaseColor = nullptr;
	ImageVk* postColor = nullptr;
	if (renderSettings_->useRT)
	{
		finalSceneColor = &compositePassHybrid_->getOutColorImage();
		finalSceneDepth = &compositePassHybrid_->getOutDepthImage();
	}
	else
	{
		finalSceneColor = &sceneColor_;
		finalSceneDepth = &sceneDepth_;
	}

	postBaseColor = finalSceneColor;

	// FOG
	if (renderSettings_->useFog)
	{
		fogPass_->setInput(
			*finalSceneDepth,
			shadowMapPass_->getDepthImage()
		);
		
		Fog_Constants::FogPassUBO fogUBO{};
		fogUBO.u_invViewProj = glm::inverse(proj * view);
		fogUBO.u_lightSpaceMatrix = shadowMapPass_->getLightSpaceMatrix();
		fogUBO.u_cameraPos = glm::vec4(in.camera->getCameraPosition(), 1.0f);
		fogUBO.u_nearFar = { in.camera->getNearPlane(), in.camera->getFarPlane() };
		fogUBO.u_fogStartEnd = { renderSettings_->fogSettings.start, renderSettings_->fogSettings.end };
		fogUBO.u_fogColor = glm::vec4(in.light->getLightColor(), 1.0f);
		fogUBO.u_lightDir = in.light->getDirection();
		fogUBO.u_maxDistance = renderSettings_->fogSettings.maxDistance;
		fogUBO.u_ambStr = in.world->getAmbientStrength();
		fogUBO.u_stepSize = renderSettings_->fogSettings.stepSize;
		fogUBO.u_scatteringDensity = renderSettings_->fogSettings.scatteringDensity;
		fogUBO.u_absorptionDensity = renderSettings_->fogSettings.absorptionDensity;

		fogPass_->render(
			frame,
			fogUBO
		);

		compositePassPost_->setInput(
			*postBaseColor,
			fogPass_->getOutputImage()
		);
		compositePassPost_->render(frame);

		postBaseColor = &compositePassPost_->getOutColorImage();
	}

	// FXAA
	if (renderSettings_->useFXAA)
	{
		fxaaPass_->setInput(*postBaseColor);
		fxaaPass_->render(frame);

		postBaseColor = &fxaaPass_->getOutputImage();
	}

	postColor = postBaseColor;
	// --------------- END POST-PROCESSING --------------- //

	// swap swapchain color image to color attachment
	frame.transitionColorImageToAttachment(cmd);

	// ----------------- PRESENT PASS ----------------- //
	if (presentPass_)
	{
		presentPass_->setInput(*postColor);
		presentPass_->render(frame);
	}
	// --------------- END PRESENT PASS --------------- //


	// ----------------- UI ELEMENTS ----------------- //
	// CROSSHAIR RENDER
	if (in.crosshair)
	{
		in.crosshair->render(&frame);
	}

	// UI RENDER
	if (ui)
	{
		ui->renderVk(frame);
	}
	// --------------- END UI ELEMENTS --------------- //

	// PRESENT TO SCREEN
	frame.transitionColorImageToPresent(cmd);
	vk_.setSwapChainLayout(frame.imageIndex, vk::ImageLayout::ePresentSrcKHR);
} // end of renderFrame()


//--- PRIVATE ---//
void RendererVk::createSceneAttachments()
{
	// SCENE COLOR
	sceneColor_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		sceneColorFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eSampled |
		vk::ImageUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	sceneColor_.createImageView(
		sceneColorFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	sceneColor_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);


	// SCENE DEPTH
	sceneDepth_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		sceneDepthFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment |
		vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	sceneDepth_.createImageView(
		sceneDepthFormat_,
		vk::ImageAspectFlagBits::eDepth,
		vk::ImageViewType::e2D,
		1
	);

	sceneDepth_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);
} // end of createSceneAttachments()
