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
WaterPassVk::WaterPassVk(VulkanMain& vk, const RenderSettings& rs)
	: vk_(vk),
	rs_(rs),
	reflColorImage_(vk), reflDepthImage_(vk),
	refrColorImage_(vk), refrDepthImage_(vk),
	dudvTex_(vk), normalTex_(vk),
	pipeline_(vk)
{
	factor_ = std::max(1u, rs_.resScale.WATER);

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
	vk::Extent2D extent = vk_.getSwapChainExtent();
	width_ = std::max(1u, (extent.width + factor_ - 1) / factor_);
	height_ = std::max(1u, (extent.height + factor_ - 1) / factor_);

	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"water/water.vert.spv",
		"water/water.frag.spv"
	);

	dudvTex_.loadFromFile("dudv.png", true);
	dudvTex_.setDebugName("WaterPassVk-DuDvTexture");

	normalTex_.loadFromFile("waternormal.png", true);
	normalTex_.setDebugName("WaterPassVk-NormalTexture");

	createAttachments();
	createResources();
	createDescriptorSet();
	createPipeline();
} // end of init()

void WaterPassVk::resize()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	if (extent.width <= 0 || extent.height <= 0) return;

	uint32_t newWidth = std::max(1u, (extent.width + factor_ - 1) / factor_);
	uint32_t newHeight = std::max(1u, (extent.height + factor_ - 1) / factor_);

	if (newWidth == width_ && newHeight == height_)
		return;

	width_ = newWidth;
	height_ = newHeight;

	const uint32_t retireFrame = vk_.getPrevFrameIndex();

	vk_.retireImage(retireFrame, std::move(reflColorImage_));
	vk_.retireImage(retireFrame, std::move(reflDepthImage_));
	vk_.retireImage(retireFrame, std::move(refrColorImage_));
	vk_.retireImage(retireFrame, std::move(refrDepthImage_));

	reflColorImage_ = ImageVk(vk_);
	reflDepthImage_ = ImageVk(vk_);
	refrColorImage_ = ImageVk(vk_);
	refrDepthImage_ = ImageVk(vk_);

	createAttachments();
	updateDescriptorSet(vk_.currentFrameIndex());
} // end of resize()

void WaterPassVk::renderOffscreen(
	const RenderSettings& rs,
	const FrameContext& frame,
	const glm::mat4& proj,
	ChunkPassVk& chunk,
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
)
{
	syncSettings();

	// refl + refr passes
	waterPass(
		rs, 
		frame, 
		proj,
		chunk, 
		in, 
		lightSpaceMatrix
	);
} // end of renderOffscreen()

void WaterPassVk::render(
	const WaterPassUBOs& ubos,
	const ChunkDrawList& drawList,
	const FrameContext& frame
)
{
	updateDescriptorSet(frame.frameIndex);

	vk::CommandBuffer cmd = frame.cmd;

	cmd.beginDebugUtilsLabelEXT({ "WaterPassVk::cmd" });

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());

	uboBuffers_[frame.frameIndex].upload(&ubos.waterData, sizeof(ubos.waterData));

	vk::DescriptorSet set = descriptorSets_[frame.frameIndex].getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline_.getLayout(),
		0,
		1, &set,
		0, nullptr
	);

	for (const auto& item : drawList.items)
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

	cmd.endDebugUtilsLabelEXT();
} // end of render()


//--- PRIVATE ---//
void WaterPassVk::syncSettings()
{
	uint32_t newFactor = std::max(1u, rs_.resScale.WATER);

	if (newFactor == factor_)
		return;

	factor_ = newFactor;
	resize();
} // end of syncSettings()

void WaterPassVk::updateDescriptorSet(uint32_t frameIndex)
{
	DescriptorSetVk& set = descriptorSets_[frameIndex];
	if (!set.valid())
	{
		return;
	}

	if (uboBuffers_[frameIndex].valid())
	{
		set.writeUniformBuffer(
			TO_API_FORM(WaterBinding::UBO),
			uboBuffers_[frameIndex].getBuffer(),
			sizeof(ChunkWaterUBO)
		);
	}

	if (reflColorImage_.valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::ReflColorTex),
			reflColorImage_.view(),
			reflColorImage_.sampler()
		);
	}

	if (refrColorImage_.valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::RefrColorTex),
			refrColorImage_.view(),
			refrColorImage_.sampler()
		);
	}

	if (refrDepthImage_.valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::RefrDepthTex),
			refrDepthImage_.view(),
			refrDepthImage_.sampler()
		);
	}

	if (dudvTex_.valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::DudvTex),
			dudvTex_.view(),
			dudvTex_.sampler()
		);
	}

	if (normalTex_.valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::NormalTex),
			normalTex_.view(),
			normalTex_.sampler()
		);
	}

	if (shadowMapTex_ && shadowMapTex_->valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(WaterBinding::ShadowTex),
			shadowMapTex_->view(),
			shadowMapTex_->sampler()
		);
	}
} // end of updateDescriptorSet()

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

	reflColorImage_.setDebugName("WaterPassVk-ReflColorImage");

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

	reflDepthImage_.setDebugName("WaterPassVk-ReflDepthImage");
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

	refrColorImage_.setDebugName("WaterPassVk-RefrColorImage");

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

	refrDepthImage_.setDebugName("WaterPassVk-RefrDepthImage");
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
			"WaterPassVK::DescriptorSet frame " + std::to_string(i)
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

	pipeline_.setDebugName("WaterPassVk::Pipeline");
} // end of createPipeline()

void WaterPassVk::waterPass(
	const RenderSettings& rs,
	const FrameContext& frame,
	const glm::mat4& proj,
	ChunkPassVk& chunk, 
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
)
{
	vk::CommandBuffer cmd{ frame.cmd };

	// REFLECTION
	cmd.beginDebugUtilsLabelEXT({ "WaterPassVk-Reflection::cmd" });
	reflColorImage_.transitionToColorAttachment(cmd);
	reflDepthImage_.transitionToDepthAttachment(cmd);

	waterReflectionPass(
		rs, 
		frame, 
		proj, 
		chunk, 
		in, 
		lightSpaceMatrix
	);

	reflColorImage_.transitionToShaderRead(cmd);
	reflDepthImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);
	cmd.endDebugUtilsLabelEXT();

	// REFRACTION
	cmd.beginDebugUtilsLabelEXT({ "WaterPassVk-Refraction::cmd" });
	refrColorImage_.transitionToColorAttachment(cmd);
	refrDepthImage_.transitionToDepthAttachment(cmd);

	waterRefractionPass(
		rs, 
		frame, 
		proj, 
		chunk, 
		in, 
		lightSpaceMatrix
	);

	refrColorImage_.transitionToShaderRead(cmd);
	refrDepthImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);
	cmd.endDebugUtilsLabelEXT();
} // end of waterPass()

void WaterPassVk::waterReflectionPass(
	const RenderSettings& rs,
	const FrameContext& frame,
	const glm::mat4& proj,
	ChunkPassVk& chunk, 
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
) const
{
	vk::CommandBuffer cmd{ frame.cmd };
	vk::Extent2D extent{ width_, height_ };

	vk::ClearValue normalClear{ {0.0f, 0.0f, 0.0f, 1.0f} };

	vk::ClearValue depthClear{ {1.0f, 0} };

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
	renderingInfo.renderArea.extent = extent;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = extent;
		cmd.setScissor(0, 1, &scissor);

		// build reflected view matrix
		const float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
		Camera camera = *in.camera;
		glm::vec3 reflectedPos = camera.getCameraPosition();
		reflectedPos.y = 2.0f * waterHeight - reflectedPos.y;
		camera.setCameraPosition(reflectedPos);
		camera.invertPitch();

		const glm::mat4 reflView = camera.getViewMatrix();
		const glm::mat4 reflProj = proj;

		// render world
		chunk.renderOpaque(
			RenderTargetVk::WaterReflection,
			in,
			frame,
			reflView,
			reflProj,
			lightSpaceMatrix,
			width_,
			height_
		);
		if (in.skybox) 
		{
			in.skybox->renderOffscreen(
				&frame,
				nullptr,
				reflView,
				proj,
				width_,
				height_,
				in.light->getDirection(),
				in.time
			);
		}
		if (in.light)
		{
			in.light->renderOffscreen(
				&frame,
				reflView,
				proj,
				width_,
				height_
			);
		}
	}
	cmd.endRendering();
} // end of waterReflectionPass()

void WaterPassVk::waterRefractionPass(
	const RenderSettings& rs,
	const FrameContext& frame,
	const glm::mat4& proj,
	ChunkPassVk& chunk, 
	const RenderInputs& in,
	const glm::mat4& lightSpaceMatrix
) const
{
	vk::CommandBuffer cmd{ frame.cmd };
	vk::Extent2D extent{ width_, height_ };

	vk::ClearValue normalClear{ {0.0f, 0.0f, 0.0f, 1.0f} };

	vk::ClearValue depthClear{ {1.0f, 0} };

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
	renderingInfo.renderArea.extent = extent;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = extent;
		cmd.setScissor(0, 1, &scissor);

		const glm::mat4 view = in.camera->getViewMatrix();

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
