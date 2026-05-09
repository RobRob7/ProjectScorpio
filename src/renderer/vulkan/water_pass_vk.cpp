#include "water_pass_vk.h"

#include "render_settings.h"
#include "frame_context_vk.h"
#include "constants.h"
#include "render_inputs.h"
#include "bindings.h"
#include "chunk_draw_list.h"
#include "i_chunk_mesh_gpu.h"

#include "chunk_pass_vk.h"
#include "camera.h"
#include "light_vk.h"
#include "cubemap_vk.h"
#include "chunk_manager.h"

#include "vulkan_main.h"

#include "vulkan/vulkan.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <memory>

using namespace Chunk_Constants;
using namespace World;

//--- PUBLIC ---//
WaterPassVk::WaterPassVk(
	VulkanMain& vk,
	const ImageVk& shadowMapTex
)
	: vk_(vk),
	shadowMapImage_(shadowMapTex),
	factor_(WATER_TEX_FACTOR),
	reflColorImage_(vk), reflDepthImage_(vk),
	refrColorImage_(vk), refrDepthImage_(vk),
	dudvTex_(vk), normalTex_(vk),
	pipeline_(vk)
{
	uboBuffers_.reserve(vk_.getMaxFramesInFlight());
	descriptorSets_.reserve(vk_.getMaxFramesInFlight());

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_.emplace_back(vk_);
		descriptorSets_.emplace_back(vk_);
	} // end for
} // end of constructor

WaterPassVk::~WaterPassVk() = default;

void WaterPassVk::init()
{
	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"water/water.vert.spv",
		"water/water.frag.spv"
	);

	vk::Extent2D extent = vk_.getSwapChainExtent();

	width_ = std::max(1u, extent.width / factor_);
	height_ = std::max(1u, extent.height / factor_);

	dudvTex_.loadFromFile("dudv.png", true);
	normalTex_.loadFromFile("waternormal.png", true);

	createAttachments();
	createResources();
	createDescriptorSet();
	createPipeline();
} // end of init()

void WaterPassVk::resize()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();

	width_ = std::max(1u, extent.width / factor_);
	height_ = std::max(1u, extent.height / factor_);

	createAttachments();
	createDescriptorSet();
} // end of resize()

void WaterPassVk::renderOffscreen(
	const RenderSettings& rs,
	const FrameContext& frame,
	ChunkPassVk& chunk,
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
)
{
	// refl + refr passes
	waterPass(rs, frame, chunk, in, lightSpaceMatrix);
} // end of renderOffscreen()

void WaterPassVk::renderWater(
	const FrameContext& frame,
	const RenderSettings& rs,
	const RenderInputs& in,
	const glm::mat4& view,
	const glm::mat4& proj,
	const glm::mat4& lightSpaceMatrix,
	int width, int height
)
{
	ChunkDrawList list;
	in.world->buildWaterDrawList(view, proj, list);

	vk::CommandBuffer cmd = frame.cmd;

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());

	vk::DescriptorSet set = descriptorSets_[frame.frameIndex].getSet();

	ChunkWaterUBO ubo{};
	ubo.u_useShadowMap = rs.useShadowMap ? 1 : 0;
	ubo.u_lightSpaceMatrix = lightSpaceMatrix;
	ubo.u_time = in.time;
	ubo.u_view = view;
	ubo.u_proj = proj;
	ubo.u_screenSize = glm::vec2(width, height);
	ubo.u_ambientStrength = in.world->getAmbientStrength();

	ubo.u_near = in.camera->getNearPlane();
	ubo.u_far = in.camera->getFarPlane();
	ubo.u_viewPos = in.camera->getCameraPosition();

	ubo.u_lightDir = in.light->getDirection();
	ubo.u_lightColor = in.light->getColor();

	uboBuffers_[frame.frameIndex].upload(&ubo, sizeof(ubo), 0);

	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline_.getLayout(),
		0,
		1, &set,
		0, nullptr
	);

	for (const auto& item : list.items)
	{
		glm::mat4 model = glm::translate(
			glm::mat4(1.0f),
			item.chunkOrigin);

		ChunkWaterPushConstants pc{};
		pc.u_model = model;

		cmd.pushConstants(
			pipeline_.getLayout(),
			vk::ShaderStageFlagBits::eVertex,
			0,
			sizeof(ChunkWaterPushConstants),
			&pc
		);

		item.gpu->drawWater(cmd);
	} // end for
} // end of renderWater()


//--- PRIVATE ---//
void WaterPassVk::createAttachments()
{
	/////////////////////////////////
	// REFLECTION COLOR
	reflColorImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		colorFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	reflColorImage_.createImageView(
		colorFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);
	reflColorImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);

	// REFLECTION DEPTH
	reflDepthImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		depthFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	reflDepthImage_.createImageView(
		depthFormat_,
		vk::ImageAspectFlagBits::eDepth,
		vk::ImageViewType::e2D,
		1
	);
	reflDepthImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);
	/////////////////////////////////

	/////////////////////////////////
	// REFRACTION COLOR
	refrColorImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		colorFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	refrColorImage_.createImageView(
		colorFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);
	refrColorImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);

	// REFRACTION DEPTH
	refrDepthImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		depthFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	refrDepthImage_.createImageView(
		depthFormat_,
		vk::ImageAspectFlagBits::eDepth,
		vk::ImageViewType::e2D,
		1
	);
	refrDepthImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);
	/////////////////////////////////
} // end of createAttachments()

void WaterPassVk::createResources()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_[i].create(
			sizeof(ChunkWaterUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void WaterPassVk::createDescriptorSet()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		vk::DescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = TO_API_FORM(WaterBinding::UBO);
		uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding reflColorBinding{};
		reflColorBinding.binding = TO_API_FORM(WaterBinding::ReflColorTex);
		reflColorBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		reflColorBinding.descriptorCount = 1;
		reflColorBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding refrColorBinding{};
		refrColorBinding.binding = TO_API_FORM(WaterBinding::RefrColorTex);
		refrColorBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		refrColorBinding.descriptorCount = 1;
		refrColorBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding refrDepthBinding{};
		refrDepthBinding.binding = TO_API_FORM(WaterBinding::RefrDepthTex);
		refrDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		refrDepthBinding.descriptorCount = 1;
		refrDepthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding dudvBinding{};
		dudvBinding.binding = TO_API_FORM(WaterBinding::DudvTex);
		dudvBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		dudvBinding.descriptorCount = 1;
		dudvBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding normalBinding{};
		normalBinding.binding = TO_API_FORM(WaterBinding::NormalTex);
		normalBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		normalBinding.descriptorCount = 1;
		normalBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding shadowMapBinding{};
		shadowMapBinding.binding = TO_API_FORM(WaterBinding::ShadowTex);
		shadowMapBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		shadowMapBinding.descriptorCount = 1;
		shadowMapBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		descriptorSets_[i].createLayout({
			uboBinding,
			reflColorBinding, 
			refrColorBinding, 
			refrDepthBinding,
			dudvBinding, 
			normalBinding, 
			shadowMapBinding
			});

		vk::DescriptorPoolSize uboPool{};
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize reflColorPool{};
		reflColorPool.type = vk::DescriptorType::eCombinedImageSampler;
		reflColorPool.descriptorCount = 1;

		vk::DescriptorPoolSize refrColorPool{};
		refrColorPool.type = vk::DescriptorType::eCombinedImageSampler;
		refrColorPool.descriptorCount = 1;

		vk::DescriptorPoolSize refrDepthPool{};
		refrDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
		refrDepthPool.descriptorCount = 1;

		vk::DescriptorPoolSize dudvPool{};
		dudvPool.type = vk::DescriptorType::eCombinedImageSampler;
		dudvPool.descriptorCount = 1;

		vk::DescriptorPoolSize normalPool{};
		normalPool.type = vk::DescriptorType::eCombinedImageSampler;
		normalPool.descriptorCount = 1;

		vk::DescriptorPoolSize shadowMapPool{};
		shadowMapPool.type = vk::DescriptorType::eCombinedImageSampler;
		shadowMapPool.descriptorCount = 1;

		descriptorSets_[i].createPool({
			uboPool,
			reflColorPool, 
			refrColorPool, 
			refrDepthPool,
			dudvPool, 
			normalPool, 
			shadowMapPool
			});
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			"WaterPassVK::descriptorSets frame " + std::to_string(i)
		);

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(WaterBinding::UBO),
			uboBuffers_[i].getBuffer(),
			sizeof(ChunkWaterUBO)
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::ReflColorTex),
			reflColorImage_.view(),
			reflColorImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::RefrColorTex),
			refrColorImage_.view(),
			refrColorImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::RefrDepthTex),
			refrDepthImage_.view(),
			refrDepthImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::DudvTex),
			dudvTex_.view(),
			dudvTex_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::NormalTex),
			normalTex_.view(),
			normalTex_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::ShadowTex),
			shadowMapImage_.view(),
			shadowMapImage_.sampler()
		);
	} // end for
} // end of createDescriptorSet()

void WaterPassVk::createPipeline()
{
	GraphicsPipelineDescVk desc{};
	desc.vertShader = shader_->vertShader();
	desc.fragShader = shader_->fragShader();

	vk::PushConstantRange pushRange{};
	pushRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
	pushRange.offset = 0;
	pushRange.size = sizeof(ChunkWaterPushConstants);
	desc.pushConstantRanges = { pushRange };

	desc.setLayouts = { descriptorSets_[0].getLayout()};

	desc.colorFormat = colorFormat_;
	desc.depthFormat = depthFormat_;

	desc.cullMode = vk::CullModeFlagBits::eBack;
	desc.frontFace = vk::FrontFace::eClockwise;
	desc.depthTestEnable = true;
	desc.depthWriteEnable = false;
	desc.depthCompareOp = vk::CompareOp::eLessOrEqual;

	vk::VertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(VertexWater);
	binding.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription attr{};
	attr.location = 0;
	attr.binding = 0;
	attr.format = vk::Format::eR32G32B32Sfloat;
	attr.offset = offsetof(VertexWater, pos);

	desc.vertexBinding = binding;
	desc.vertexAttributes = { attr };

	pipeline_.create(desc);
} // end of createPipeline()

void WaterPassVk::waterPass(
	const RenderSettings& rs,
	const FrameContext& frame,
	ChunkPassVk& chunk, 
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
)
{
	vk::CommandBuffer cmd = frame.cmd;

	// REFLECTION
	reflColorImage_.transitionToColorAttachment(cmd);
	reflDepthImage_.transitionToDepthAttachment(cmd);

	waterReflectionPass(rs, frame, chunk, in, lightSpaceMatrix);

	reflColorImage_.transitionToShaderRead(cmd);
	reflDepthImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);


	// REFRACTION
	refrColorImage_.transitionToColorAttachment(cmd);
	refrDepthImage_.transitionToDepthAttachment(cmd);

	waterRefractionPass(rs, frame, chunk, in, lightSpaceMatrix);

	refrColorImage_.transitionToShaderRead(cmd);
	refrDepthImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);
} // end of waterPass()

void WaterPassVk::waterReflectionPass(
	const RenderSettings& rs,
	const FrameContext& frame,
	ChunkPassVk& chunk, 
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
) const
{
	vk::CommandBuffer cmd = frame.cmd;

	vk::ClearValue normalClear{};
	normalClear.color.float32[0] = 0.0f;
	normalClear.color.float32[1] = 0.0f;
	normalClear.color.float32[2] = 0.0f;
	normalClear.color.float32[3] = 1.0f;

	vk::ClearValue depthClear{};
	depthClear.depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

	vk::RenderingAttachmentInfo colorAttachment{};
	colorAttachment.imageView = reflColorImage_.view();
	colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.clearValue = normalClear;

	vk::RenderingAttachmentInfo depthAttachment{};
	depthAttachment.imageView = reflDepthImage_.view();
	depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	depthAttachment.clearValue = depthClear;

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = vk::Extent2D{
		static_cast<uint32_t>(width_),
		static_cast<uint32_t>(height_)
	};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(width_);
		viewport.height = static_cast<float>(height_);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = vk::Extent2D{
			static_cast<uint32_t>(width_),
			static_cast<uint32_t>(height_)
		};
		cmd.setScissor(0, 1, &scissor);

		// build reflected view matrix
		const float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
		Camera camera = *in.camera;
		glm::vec3 reflectedPos = camera.getCameraPosition();
		reflectedPos.y = 2.0f * waterHeight - reflectedPos.y;
		camera.setCameraPosition(reflectedPos);
		camera.invertPitch();

		const glm::mat4 reflView = camera.getViewMatrix();

		// set clip plane (clip everything below water)
		glm::vec4 clipPlane{ 0, 1, 0, -waterHeight };

		const float aspect = (height_ > 0)
			? (static_cast<float>(width_) / static_cast<float>(height_))
			: 1.0f;
		glm::mat4 proj = camera.getProjectionMatrixVk(aspect);
		proj[1][1] *= -1.0f;

		// render world
		chunk.renderOpaque(
			RenderTargetVk::WaterReflection,
			in,
			frame,
			reflView,
			proj,
			lightSpaceMatrix,
			width_,
			height_
		);
		if (in.skybox) 
		{
			in.skybox->renderOffscreen(
				&frame,
				reflView,
				proj,
				width_,
				height_,
				in.light->getDirection(),
				in.time
			);
		}
	}
	cmd.endRendering();
} // end of waterReflectionPass()

void WaterPassVk::waterRefractionPass(
	const RenderSettings& rs,
	const FrameContext& frame,
	ChunkPassVk& chunk, 
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
) const
{
	vk::CommandBuffer cmd = frame.cmd;

	vk::ClearValue normalClear{};
	normalClear.color.float32[0] = 0.0f;
	normalClear.color.float32[1] = 0.0f;
	normalClear.color.float32[2] = 0.0f;
	normalClear.color.float32[3] = 1.0f;

	vk::ClearValue depthClear{};
	depthClear.depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

	vk::RenderingAttachmentInfo colorAttachment{};
	colorAttachment.imageView = refrColorImage_.view();
	colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.clearValue = normalClear;

	vk::RenderingAttachmentInfo depthAttachment{};
	depthAttachment.imageView = refrDepthImage_.view();
	depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	depthAttachment.clearValue = depthClear;

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = vk::Extent2D{
		static_cast<uint32_t>(width_),
		static_cast<uint32_t>(height_)
	};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(width_);
		viewport.height = static_cast<float>(height_);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = vk::Extent2D{
			static_cast<uint32_t>(width_),
			static_cast<uint32_t>(height_)
		};
		cmd.setScissor(0, 1, &scissor);

		// set clip plane (clip everything above water)
		float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
		glm::vec4 clipPlane{ 0, -1, 0, waterHeight };

		const glm::mat4 view = in.camera->getViewMatrix();
		const float aspect = (height_ > 0)
			? (static_cast<float>(width_) / static_cast<float>(height_))
			: 1.0f;
		glm::mat4 proj = in.camera->getProjectionMatrixVk(aspect);
		proj[1][1] *= -1.0f;

		// render world
		chunk.renderOpaque(
			RenderTargetVk::WaterRefraction,
			in,
			frame,
			view,
			proj,
			lightSpaceMatrix,
			width_,
			height_
		);
	}
	cmd.endRendering();
} // end of waterRefractionPass()
