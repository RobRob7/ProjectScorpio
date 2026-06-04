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

#include "ray_tracing_world_vk.h"
#include "rtao_pass_vk.h"
#include "rt_shadow_pass_vk.h"
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
	if (!rs_) 
	{
		rs_ = std::make_unique<RenderSettings>();
	}

	if (vk_.supportsRayTracing())
	{
		if (!rtWorld_)
		{
			rtWorld_ = std::make_unique<RayTracingWorldVk>(vk_);
		}

		if (!rtaoPass_)
		{
			rtaoPass_ = std::make_unique<RTAOPassVk>(
				vk_,
				*rs_,
				rtWorld_->getTLAS()
			);
		}
		if (!rtShadowPass_)
		{
			rtShadowPass_ = std::make_unique<RTShadowPassVk>(
				vk_,
				*rs_,
				rtWorld_->getTLAS()
			);
		}

		if (!rtWorldPass_)
		{
			rtWorldPass_ = std::make_unique<RayTracingWorldPassVk>(
				vk_,
				*rs_,
				rtWorld_->getTLAS(),
				rtWorld_->getPackedRTOpaqueInfoBuffer(),
				rtWorld_->getPackedRTOpaqueInfoBufferSize(),
				rtWorld_->getPackedRTWaterInfoBuffer(),
				rtWorld_->getPackedRTWaterInfoBufferSize()
			);
		}
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
			vk_
			//gbufferPass_->getNormalImage(),
			//gbufferPass_->getDepthImage(),
			//shadowMapPass_->getDepthImage(),
			//shadowMapPass_->getDepthImage()
			////rtWorldPass_->getOutDepthImage()
		);
	}
	if (!ssaoPass_)
	{
		ssaoPass_ = std::make_unique<SSAOPassVk>(vk_, *rs_);
	}

	if (!waterPass_)
	{
		waterPass_ = std::make_unique<WaterPassVk>(vk_, *rs_);
	}
	if (!chunkPass_)
	{
		chunkPass_ = std::make_unique<ChunkPassVk>(vk_, *rs_);
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
		fogPass_ = std::make_unique<FogPassVk>(vk_, *rs_);
	}
	if (!fxaaPass_)
	{
		fxaaPass_ = std::make_unique<FXAAPassVk>(vk_);
	}

	if (!presentPass_)
	{
		presentPass_ = std::make_unique<PresentPassVk>(vk_);
	}

	if (rtaoPass_)
	{
		rtaoPass_->init();
	}
	if (rtShadowPass_)
	{
		rtShadowPass_->init();
	}
	if (rtWorldPass_)
	{
		rtWorldPass_->init();
	}
	else
	{
		rs_->useRT = false;
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

	if (rtaoPass_)		rtaoPass_->resize();
	if (rtShadowPass_)	rtShadowPass_->resize();
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
		rs_->sunPaused
	);

	// update world state
	in.world->updateDynamic(in.camera->getCameraPosition(), &frame);
	if (rs_->useRT)
	{
		in.world->buildRTDrawList(view, proj);
	}
	else
	{
		in.world->buildWaterDrawList(view, proj);
	}

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

	// RT upload
	if (vk_.supportsRayTracing() && rs_->useRT && rtWorld_)
	{
		rtWorld_->upload(
			cmd,
			in.world->getRTDrawList(),
			frame.frameIndex
		);

		if (rtaoPass_)
		{
			rtaoPass_->setInput(
				frame.frameIndex,
				gbufferPass_->getNormalImage(),
				gbufferPass_->getDepthImage()
			);
		}
		if (rtShadowPass_)
		{
			rtShadowPass_->setInput(
				frame.frameIndex,
				gbufferPass_->getNormalImage(),
				gbufferPass_->getDepthImage()
			);
		}
	}

	// RTAO pass
	if (rtaoPass_)
	{
		RTAOPassUBOs ubos
		{
			.rayGenData = {
				.u_invView = glm::inverse(view),
				.u_invViewProj = glm::inverse(proj * view),
				.u_useRTAO = rs_->useRTAO ? 1 : 0,
				.u_AORadius = rs_->aoSettings.radius,
				.u_AOSamples = rs_->aoSettings.samples
			}
		};
		rtaoPass_->render(ubos, frame);
	}

	// RT shadow pass
	if (rtShadowPass_)
	{
		RTShadowPassUBOs ubos
		{
			.rayGenData = {
				.u_invView = glm::inverse(view),
				.u_invViewProj = glm::inverse(proj * view),
				.u_lightDir = in.light->getDirection(),
				.u_useRTShadows = rs_->useRTShadow ? 1 : 0
			}
		};
		rtShadowPass_->render(ubos, frame);
	}

	// shadow map pass
	if ((!rs_->useRT && shadowMapPass_) || rs_->useFog)
	{
		shadowMapPass_->render(
			*chunkPass_,
			in,
			frame
		);
	}

	// ssao pass
	if (!rs_->useRT && rs_->useSSAO)
	{
		ssaoPass_->setInput(
			gbufferPass_->getNormalImage(), 
			gbufferPass_->getDepthImage()
		);

		vk::Extent2D ssaoExtent = ssaoPass_->getExtent();
		SSAOPassUBOs ubos
		{
			.blurData = {
				.u_texelSize = glm::vec2(
					1.0f / static_cast<float>(ssaoExtent.width),
					1.0f / static_cast<float>(ssaoExtent.height)
				)
			},
			.rawData = {
				.u_proj = proj,
				.u_invProj = glm::inverse(proj),
				.u_noiseScale = glm::vec2(
					static_cast<float>(ssaoExtent.width) / SSAO_Constants::K_NOISE_SIZE,
					static_cast<float>(ssaoExtent.height) / SSAO_Constants::K_NOISE_SIZE
				),
				.u_radius = rs_->aoSettings.radius,
				.u_bias = SSAO_Constants::BIAS,
				.u_kernelSize = rs_->aoSettings.samples,
			}
		};
		std::memcpy(
			ubos.rawSamplesData.u_samples,
			ssaoPass_->getSamples().data(),
			sizeof(ubos.rawSamplesData.u_samples)
		);
		ssaoPass_->render(ubos, frame);
	}

	// water refl + refr pass
	if (!rs_->useRT && waterPass_)
	{

		waterPass_->renderOffscreen(
			*rs_,
			frame,
			proj,
			*chunkPass_,
			in,
			shadowMapPass_->getLightSpaceMatrix()
		);
	}

	// debug pass
	if (rs_->debugMode != DebugMode::None)
	{
		//debugPass_->setInput(
		//	rtShadowPass_->getOutColorImage(),
		//	//gbufferPass_->getNormalImage(),
		//	gbufferPass_->getDepthImage(),
		//	shadowMapPass_->getDepthImage(),
		//	//shadowMapPass_->getDepthImage()
		//	rtWorldPass_->getOutDepthImage()
		//);
		//debugPass_->updateDescriptorSet(frame.frameIndex);

		//debugPass_->render(
		//	frame,
		//	in.camera->getNearPlane(),
		//	in.camera->getFarPlane(),
		//	static_cast<int>(rs_->debugMode)
		//);

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

		if (chunkPass_ && !rs_->useRT)
		{
			chunkPass_->setInput(
				ssaoPass_->ssaoBlurImage(),
				shadowMapPass_->getDepthImage()
			);

			chunkPass_->renderOpaque(
				RenderTargetVk::Default,
				in,
				frame,
				view,
				proj,
				shadowMapPass_->getLightSpaceMatrix()
			);
		}

		if (waterPass_ && !rs_->useRT)
		{
			waterPass_->setInput(shadowMapPass_->getDepthImage());

			WaterPassUBOs ubos
			{
				.waterData = {
					.u_lightSpaceMatrix = shadowMapPass_->getLightSpaceMatrix(),
					.u_view = view,
					.u_proj = proj,

					.u_time = in.time,
					.u_useShadowMap = rs_->useShadowMap ? 1 : 0,
					.u_near = in.camera->getNearPlane(),
					.u_far = in.camera->getFarPlane(),
					.u_screenSize = glm::vec2(width_, height_),
					.u_viewPos = in.camera->getCameraPosition(),
					.u_lightDir = in.light->getDirection(),
					.u_ambientStrength = in.world->getAmbientStrength()
				}
			};
			waterPass_->render(ubos, in.world->getWaterDrawList(), frame);
		}

		if (in.skybox && !rs_->useRT) 
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
	if (vk_.supportsRayTracing() && rs_->useRT && rtWorldPass_)
	{
		CubemapVk* skybox = dynamic_cast<CubemapVk*>(in.skybox);
		rtWorldPass_->setSkybox(
			frame.frameIndex,
			skybox->getNightTexture(),
			skybox->getDayTexture()
		);
		rtWorldPass_->setRTAOTexture(rtaoPass_->getOutColorImage());
		rtWorldPass_->setRTShadowTexture(rtShadowPass_->getOutColorImage());

		rtWorldPass_->updateDescriptorSet(frame.frameIndex);

		RayTracingWorldPassUBOs ubos
		{
			.rayGenData = {
				.u_invView = glm::inverse(view),
				.u_invProj = glm::inverse(proj),
				.u_view = view,
				.u_proj = proj,
				.u_cameraPos = glm::vec4(in.camera->getCameraPosition(), 1.0f)
			},
			.missData = {
				.u_mix = glm::vec4(
					glm::clamp((in.light->getDirection().y + 0.15f) / 0.30f, 0.0f, 1.0f), 
					1.0f, 1.0f, 1.0f)
			},
			.closestHitOpaqueData = {
				.u_lightDir = glm::vec4(in.light->getDirection(), 0.0f),
				.u_lightColor = glm::vec4(in.light->getLightColor(), 0.0f),
				.u_ambStr = in.world->getAmbientStrength()
			},
			.closestHitWaterData = {
				.u_lightDir = glm::vec4(in.light->getDirection(), 0.0f),
				.u_lightColor = glm::vec4(in.light->getLightColor(), 0.0f),
				.u_time = in.time
			}
		};
		rtWorldPass_->render(frame, ubos);
	}
	// --------------- END FORWARD RENDER --------------- //

	// scene color + depth transition to shader read
	sceneColor_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eColor);
	sceneDepth_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);

	// ----------------- HYBRID COMPOSITE PASS ----------------- //
	if (vk_.supportsRayTracing() && rs_->useRT)
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
	if (rs_->useRT)
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
	if (rs_->useFog)
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
		fogUBO.u_fogStartEnd = { rs_->fogSettings.start, rs_->fogSettings.end };
		fogUBO.u_fogColor = glm::vec4(in.light->getLightColor(), 1.0f);
		fogUBO.u_lightDir = in.light->getDirection();
		fogUBO.u_maxDistance = rs_->fogSettings.maxDistance;
		fogUBO.u_ambStr = in.world->getAmbientStrength();
		fogUBO.u_stepSize = rs_->fogSettings.stepSize;
		fogUBO.u_scatteringDensity = rs_->fogSettings.scatteringDensity;
		fogUBO.u_absorptionDensity = rs_->fogSettings.absorptionDensity;

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
	if (rs_->useFXAA)
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

	sceneColor_.setDebugName("RendererVk-SceneColor");


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

	sceneDepth_.setDebugName("RendererVk-SceneDepth");
} // end of createSceneAttachments()
