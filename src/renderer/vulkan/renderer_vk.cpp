#include "renderer_vk.h"

#include "frame_context_vk.h"

#include "utils_vk.h"
#include "vulkan_main.h"

#include "render_settings.h"
#include "render_inputs.h"

#include "camera.h"
#include "i_light.h"
#include "i_cubemap.h"
#include "i_crosshair.h"
#include "chunk_manager.h"
#include "ui.h"

#include "ray_tracing_pass_vk.h"
#include "gbuffer_pass_vk.h"
#include "shadow_map_pass_vk.h"
#include "debug_pass_vk.h"
#include "ssao_pass_vk.h"
#include "water_pass_vk.h"
#include "chunk_pass_vk.h"
#include "fxaa_pass_vk.h"
#include "fog_pass_vk.h"
#include "present_pass_vk.h"

#include <glm/glm.hpp>

//--- PUBLIC ---//
RendererVk::RendererVk(VulkanMain& vk)
	: vk_(vk),
	sceneColor_(vk),
	sceneDepth_(vk),
	topLevelAS_(vk)
{
} // end of constructor

RendererVk::~RendererVk() = default;

void RendererVk::init()
{
	if (!renderSettings_) renderSettings_ = std::make_unique<RenderSettings>();

	if (!rayTracingPass_)	rayTracingPass_ = std::make_unique<RayTracingPassVk>(vk_);

	if (!gbufferPass_)		gbufferPass_ = std::make_unique<GBufferPassVk>(vk_);
	if (!shadowMapPass_)	shadowMapPass_ = std::make_unique<ShadowMapPassVk>(vk_);
	if (!debugPass_)		debugPass_ = std::make_unique<DebugPassVk>(vk_, gbufferPass_->getNormalImage(), gbufferPass_->getDepthImage(), shadowMapPass_->getImage());
	if (!ssaoPass_)			ssaoPass_ = std::make_unique<SSAOPassVk>(vk_, gbufferPass_->getNormalImage(), gbufferPass_->getDepthImage());

	if (!waterPass_)		waterPass_ = std::make_unique<WaterPassVk>(vk_, shadowMapPass_->getImage());
	if (!chunkPass_)		chunkPass_ = std::make_unique<ChunkPassVk>(vk_, ssaoPass_->ssaoBlurImage(), shadowMapPass_->getImage());

	if (!fxaaPass_)			fxaaPass_ = std::make_unique<FXAAPassVk>(vk_);
	if (!fogPass_)			fogPass_ = std::make_unique<FogPassVk>(vk_, *renderSettings_);
	if (!presentPass_)		presentPass_ = std::make_unique<PresentPassVk>(vk_);

	rayTracingPass_->init();

	gbufferPass_->init();
	shadowMapPass_->init();
	debugPass_->init();
	ssaoPass_->init();

	waterPass_->init();
	chunkPass_->init();
	chunkPass_->refreshTexBinding();

	fxaaPass_->init();
	fogPass_->init();
	presentPass_->init();
} // end of init()

void RendererVk::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (w == width_ && h == height_) return;

	width_ = w;
	height_ = h;

	if (rayTracingPass_) rayTracingPass_->resize();

	if (gbufferPass_)	gbufferPass_->resize(width_, height_);
	if (debugPass_)		debugPass_->resize(width_, height_);
	if (ssaoPass_)		ssaoPass_->resize(width_, height_);

	if (waterPass_)		waterPass_->resize(width_, height_);
	if (chunkPass_)		chunkPass_->refreshTexBinding();

	if (fxaaPass_)		fxaaPass_->resize(width_, height_);
	if (fogPass_)		fogPass_->resize(width_, height_);

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

	// update light direction
	in.light->updateLightDirection(in.time);

	in.world->update(in.camera->getCameraPosition());

	const glm::mat4 view = in.camera->getViewMatrix();
	const float aspect = (height_ > 0)
		? (static_cast<float>(width_) / static_cast<float>(height_))
		: 1.0f;
	glm::mat4 proj = in.camera->getProjectionMatrix(aspect);
	proj[1][1] *= -1.0f;

	vk::CommandBuffer cmd = frame.cmd;

	// update world TLAS
	std::vector<vk::AccelerationStructureInstanceKHR> instances;
	in.world->buildTLASInstances(instances);
	if (!instances.empty())
	{
		topLevelAS_.buildTLAS(instances);
		rayTracingPass_->setTopLevelAS(topLevelAS_.handle());
	}

	// RT test
	if (rayTracingPass_)
	{
		VkUtils::TransitionImageLayout(
			cmd,
			rayTracingPass_->getOutputImageVk().image(),
			vk::ImageAspectFlagBits::eColor,
			rayTracingPass_->getOutputLayout(),
			vk::ImageLayout::eGeneral,
			1,
			1
		);

		rayTracingPass_->render(frame);

		VkUtils::TransitionImageLayout(
			cmd,
			rayTracingPass_->getOutputImageVk().image(),
			vk::ImageAspectFlagBits::eColor,
			rayTracingPass_->getOutputLayout(),
			vk::ImageLayout::eShaderReadOnlyOptimal,
			1,
			1
		);
	}


	VkUtils::TransitionImageLayout(
		cmd,
		frame.colorImage,
		vk::ImageAspectFlagBits::eColor,
		frame.colorLayout,
		vk::ImageLayout::eColorAttachmentOptimal,
		1,
		1
	);

	if (presentPass_)
	{
		presentPass_->setInput(rayTracingPass_->getOutputImageVk());
		presentPass_->render(frame);
	}

	if (ui)
	{
		ui->renderVk(frame);
	}

	VkUtils::TransitionImageLayout(
		cmd,
		frame.colorImage,
		vk::ImageAspectFlagBits::eColor,
		frame.colorLayout,
		vk::ImageLayout::ePresentSrcKHR,
		1,
		1
	);
	vk_.setSwapChainLayout(frame.imageIndex, vk::ImageLayout::ePresentSrcKHR);
	return;



	// ----------------- PASSES ----------------- //
	// gbuffer pass
	if (gbufferPass_)
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
	if (shadowMapPass_)
	{
		shadowMapPass_->renderOffscreen(
			*chunkPass_,
			in,
			frame
		);
	}

	// debug pass
	if (renderSettings_->debugMode != DebugMode::None)
	{
		vk::ImageLayout old = vk_.getSwapChainLayout(frame.imageIndex);

		debugPass_->render(
			frame,
			old,
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

	// ssao pass
	if (renderSettings_->useSSAO)
	{
		ssaoPass_->renderOffscreen(frame, proj);
	}

	// water refl + refr pass
	if (waterPass_)
	{
		waterPass_->renderOffscreen(
			*renderSettings_,
			frame,
			*chunkPass_,
			in,
			shadowMapPass_->getLightSpaceMatrix()
		);
	}
	// --------------- END PASSES --------------- //


	// ----------------- FORWARD RENDER ----------------- //
	// scene color transition to attachment
	VkUtils::TransitionImageLayout(
		cmd,
		sceneColor_.image(),
		vk::ImageAspectFlagBits::eColor,
		sceneColorLayout_,
		vk::ImageLayout::eColorAttachmentOptimal,
		1,
		1
	);

	// scene depth transition to attachment
	VkUtils::TransitionImageLayout(
		cmd,
		sceneDepth_.image(),
		vk::ImageAspectFlagBits::eDepth,
		sceneDepthLayout_,
		vk::ImageLayout::eDepthAttachmentOptimal,
		1,
		1
	);

	vk::ClearValue clear{};
	clear.color.float32[0] = 0.0f;
	clear.color.float32[1] = 0.0f;
	clear.color.float32[2] = 0.0f;
	clear.color.float32[3] = 1.0f;

	vk::RenderingAttachmentInfo colorAttach{};
	colorAttach.imageView = sceneColor_.view();
	colorAttach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttach.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttach.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttach.clearValue = clear;

	vk::RenderingAttachmentInfo depthAttach{};
	depthAttach.imageView = sceneDepth_.view();
	depthAttach.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
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

		if (chunkPass_)
		{
			Chunk_Constants::ChunkOpaqueUBO ubo{};
			chunkPass_->renderOpaque(
				in, 
				*renderSettings_, 
				frame, 
				view, 
				proj, 
				shadowMapPass_->getLightSpaceMatrix(),
				width_, 
				height_, 
				ubo
			);
		}

		if (in.skybox) in.skybox->render(
			&frame, 
			view,
			proj,
			in.light->getDirection(),
			in.time
		);

		if (waterPass_)
		{
			waterPass_->renderWater(
				*renderSettings_,
				in,
				cmd,
				view,
				proj,
				shadowMapPass_->getLightSpaceMatrix(),
				width_,
				height_
			);
		}
	}
	cmd.endRendering();

	// scene color transition to shader read
	VkUtils::TransitionImageLayout(
		cmd,
		sceneColor_.image(),
		vk::ImageAspectFlagBits::eColor,
		sceneColorLayout_,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		1,
		1
	);
	// scene depth transition to shader read
	VkUtils::TransitionImageLayout(
		cmd,
		sceneDepth_.image(),
		vk::ImageAspectFlagBits::eDepth,
		sceneDepthLayout_,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		1,
		1
	);
	// --------------- END FORWARD RENDER --------------- //


	// ----------------- POST-PROCESSING ----------------- //
	ImageVk* postColor = &sceneColor_;
	ImageVk* postDepth = &sceneDepth_;
	// FXAA
	if (renderSettings_->useFXAA)
	{
		fxaaPass_->setInput(*postColor);
		fxaaPass_->render(frame);
		postColor = &fxaaPass_->getOutputImage();
	}

	// FOG
	if (renderSettings_->useFog)
	{
		fogPass_->setInput(*postColor, *postDepth);
		fogPass_->render(
			frame,
			in.camera->getNearPlane(),
			in.camera->getFarPlane(),
			in.world->getAmbientStrength()
		);
		postColor = &fogPass_->getOutputImage();
	}
	// --------------- END POST-PROCESSING --------------- //


	// swap swapchain color image to color attachment
	VkUtils::TransitionImageLayout(
		cmd,
		frame.colorImage,
		vk::ImageAspectFlagBits::eColor,
		frame.colorLayout,
		vk::ImageLayout::eColorAttachmentOptimal,
		1,
		1
	);


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
	VkUtils::TransitionImageLayout(
		cmd, 
		frame.colorImage,
		vk::ImageAspectFlagBits::eColor,
		frame.colorLayout,
		vk::ImageLayout::ePresentSrcKHR,
		1,
		1
	);
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
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
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
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
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

	sceneColorLayout_ = vk::ImageLayout::eUndefined;
	sceneDepthLayout_ = vk::ImageLayout::eUndefined;
} // end of createSceneAttachments()