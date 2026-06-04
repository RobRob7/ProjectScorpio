#include "ssao_pass_vk.h"

#include "bindings.h"
#include "constants.h"

#include "render_settings.h"
#include "frame_context_vk.h"
#include "shader_vk.h"
#include "utils_vk.h"

#include "vulkan_main.h"

#include <vulkan/vulkan.hpp>

#include <random>
#include <vector>
#include <memory>
#include <cstdint>

//--- PUBLIC ---//
SSAOPassVk::SSAOPassVk(VulkanMain& vk, const RenderSettings& rs)
	: vk_(vk),
	rs_(rs),
	ssaoNoiseImage_(vk),
	ssaoRawImage_(vk),
	ssaoBlurImage_(vk),
	ssaoRawPipeline_(vk),
	ssaoBlurPipeline_(vk)
{
	factor_ = std::max(1u, rs_.resScale.SSAO);

	ssaoRawSamplesUBOBuffers_.reserve(vk_.getMaxFramesInFlight());
	ssaoRawUBOBuffers_.reserve(vk.getMaxFramesInFlight());
	ssaoRawDescriptorSets_.reserve(vk.getMaxFramesInFlight());
	ssaoBlurUBOBuffers_.reserve(vk.getMaxFramesInFlight());
	ssaoBlurDescriptorSets_.reserve(vk.getMaxFramesInFlight());
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		ssaoRawSamplesUBOBuffers_.emplace_back(vk_);
		ssaoRawUBOBuffers_.emplace_back(vk_);
		ssaoRawDescriptorSets_.emplace_back(vk_);
		ssaoBlurUBOBuffers_.emplace_back(vk_);
		ssaoBlurDescriptorSets_.emplace_back(vk_);
	} // end for
	
} // end of constructor

SSAOPassVk::~SSAOPassVk() = default;

void SSAOPassVk::init()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	width_ = std::max(1u, (extent.width + factor_ - 1) / factor_);
	height_ = std::max(1u, (extent.height + factor_ - 1) / factor_);

	ssaoRawShader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"ssaopass/ssao.vert.spv", 
		"ssaopass/ssao.frag.spv"
	);
	ssaoBlurShader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"ssaopass/ssaoblur.vert.spv",
		"ssaopass/ssaoblur.frag.spv"
	);

	createNoiseTexture();
	createAttachments();
	createResources();
	createKernel();
	createDescriptorSets();
	createPipelines();
} // end of init()

void SSAOPassVk::resize()
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

	vk_.retireImage(retireFrame, std::move(ssaoRawImage_));
	vk_.retireImage(retireFrame, std::move(ssaoBlurImage_));

	ssaoRawImage_ = ImageVk(vk_);
	ssaoBlurImage_ = ImageVk(vk_);

	createAttachments();
	updateDescriptorSet(vk_.currentFrameIndex());
} // end of resize()

void SSAOPassVk::render(
	const SSAOPassUBOs& ubos,
	const FrameContext& frame
)
{
	syncSettings();

	if (!gNormalTex_ ||
		!gDepthTex_ ||
		!ssaoRawImage_.valid() ||
		!ssaoBlurImage_.valid())
	{
		return;
	}

	updateDescriptorSet(frame.frameIndex);

	vk::CommandBuffer cmd = frame.cmd;
	vk::Extent2D extent = { width_, height_ };

	// SSAO RAW RENDER
	{
		cmd.beginDebugUtilsLabelEXT({ "SSAOPassVk-Raw::cmd" });

		ssaoRawImage_.transitionToColorAttachment(cmd);

		vk::ClearValue aoClear{ {1.0f, 0.0f, 0.0f, 0.0f} };

		vk::RenderingAttachmentInfo colorAttachment{};
		colorAttachment.imageView = ssaoRawImage_.view();
		colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.clearValue = aoClear;

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
		renderingInfo.renderArea.extent = extent;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

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

			ssaoRawSamplesUBOBuffers_[frame.frameIndex].upload(&ubos.rawSamplesData, sizeof(ubos.rawSamplesData));
			ssaoRawUBOBuffers_[frame.frameIndex].upload(&ubos.rawData, sizeof(ubos.rawData));

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ssaoRawPipeline_.getPipeline());

			vk::DescriptorSet set = ssaoRawDescriptorSets_[frame.frameIndex].getSet();

			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				ssaoRawPipeline_.getLayout(),
				0,
				1,
				&set,
				0,
				nullptr
			);

			cmd.draw(3, 1, 0, 0);
		}
		cmd.endRendering();

		ssaoRawImage_.transitionToShaderRead(cmd);

		cmd.endDebugUtilsLabelEXT();
	}

	// SSAO BLUR RENDER
	{
		cmd.beginDebugUtilsLabelEXT({ "SSAOPassVk-Blur::cmd" });

		ssaoBlurImage_.transitionToColorAttachment(cmd);

		vk::ClearValue aoClear{ {1.0f, 0.0f, 0.0f, 0.0f} };

		vk::RenderingAttachmentInfo colorAttachment{};
		colorAttachment.imageView = ssaoBlurImage_.view();
		colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachment.clearValue = aoClear;

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
		renderingInfo.renderArea.extent = extent;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

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

			ssaoBlurUBOBuffers_[frame.frameIndex].upload(&ubos.blurData, sizeof(ubos.blurData));

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ssaoBlurPipeline_.getPipeline());

			vk::DescriptorSet set = ssaoBlurDescriptorSets_[frame.frameIndex].getSet();
			cmd.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				ssaoBlurPipeline_.getLayout(),
				0,
				1, &set,
				0, nullptr
			);

			cmd.draw(3, 1, 0, 0);
		}
		cmd.endRendering();

		ssaoBlurImage_.transitionToShaderRead(cmd);

		cmd.endDebugUtilsLabelEXT();
	}
} // end of render()


//--- PRIVATE ---//
void SSAOPassVk::syncSettings()
{
	uint32_t newFactor = std::max(1u, rs_.resScale.SSAO);

	if (newFactor == factor_)
		return;

	factor_ = newFactor;
	resize();
} // end of syncSettings()

void SSAOPassVk::updateDescriptorSet(uint32_t frameIndex)
{
	// RAW SET
	{
		DescriptorSetVk& set = ssaoRawDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (ssaoRawSamplesUBOBuffers_[frameIndex].valid())
		{
			set.writeUniformBuffer(
				TO_API_FORM(SSAORawBinding::SamplesUBO),
				ssaoRawSamplesUBOBuffers_[frameIndex].getBuffer(),
				sizeof(SSAO_Constants::SSAORawSamplesUBO)
			);
		}

		if (ssaoRawUBOBuffers_[frameIndex].valid())
		{
			set.writeUniformBuffer(
				TO_API_FORM(SSAORawBinding::UBO),
				ssaoRawUBOBuffers_[frameIndex].getBuffer(),
				sizeof(SSAO_Constants::SSAORawUBO)
			);
		}

		if (gNormalTex_ && gNormalTex_->valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(SSAORawBinding::GNormalTex),
				gNormalTex_->view(),
				gNormalTex_->sampler()
			);
		}

		if (gDepthTex_ && gDepthTex_->valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(SSAORawBinding::GDepthTex),
				gDepthTex_->view(),
				gDepthTex_->sampler()
			);
		}

		if (ssaoNoiseImage_.valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(SSAORawBinding::NoiseTex),
				ssaoNoiseImage_.view(),
				ssaoNoiseImage_.sampler()
			);
		}
	}

	// BLUR SET
	{
		DescriptorSetVk& set = ssaoBlurDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (ssaoBlurUBOBuffers_[frameIndex].valid())
		{
			set.writeUniformBuffer(
				TO_API_FORM(SSAOBlurBinding::UBO),
				ssaoBlurUBOBuffers_[frameIndex].getBuffer(),
				sizeof(SSAO_Constants::SSAOBlurUBO)
			);
		}

		if (ssaoRawImage_.valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(SSAOBlurBinding::SSAORawTex),
				ssaoRawImage_.view(),
				ssaoRawImage_.sampler()
			);
		}
	}
} // end of updateDescriptorSet()

void SSAOPassVk::createAttachments()
{
	// RAW
	ssaoRawImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		singleChannelFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	ssaoRawImage_.createImageView(
		singleChannelFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);
	ssaoRawImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);
	ssaoRawImage_.setDebugName("SSAOPassVk-RawImage");


	// BLUR
	ssaoBlurImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		singleChannelFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	ssaoBlurImage_.createImageView(
		singleChannelFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);
	ssaoBlurImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);
	ssaoBlurImage_.setDebugName("SSAOPassVk-BlurImage");

	// default ssaoblur texture (for when SSAO is disabled)
	vk::CommandBuffer cmd = vk_.beginSingleTimeCommands();

	ssaoBlurImage_.transitionToColorAttachment(cmd);

	vk::ClearValue clear{ {1.0f, 0.0f, 0.0f, 0.0f} };

	vk::RenderingAttachmentInfo colorAttachment{};
	colorAttachment.imageView = ssaoBlurImage_.view();
	colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.clearValue = clear;

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = vk::Extent2D{
		ssaoBlurImage_.width(),
		ssaoBlurImage_.height()
	};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	cmd.beginRendering(renderingInfo);
	cmd.endRendering();

	ssaoBlurImage_.transitionToShaderRead(cmd);

	vk_.endSingleTimeCommands(cmd);
} // end of createAttachments()

void SSAOPassVk::createResources()
{
	for (auto& buffer : ssaoRawSamplesUBOBuffers_)
	{
		buffer.create(
			sizeof(SSAO_Constants::SSAORawSamplesUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for

	for (auto& buffer : ssaoRawUBOBuffers_)
	{
		buffer.create(
			sizeof(SSAO_Constants::SSAORawUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for

	for (auto& buffer : ssaoBlurUBOBuffers_)
	{
		buffer.create(
			sizeof(SSAO_Constants::SSAOBlurUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void SSAOPassVk::createDescriptorSets()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		// RAW DS
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(SSAORawBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
			
			vk::DescriptorSetLayoutBinding uboSamplesBinding{};
			uboSamplesBinding.binding = TO_API_FORM(SSAORawBinding::SamplesUBO);
			uboSamplesBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboSamplesBinding.descriptorCount = 1;
			uboSamplesBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding gNormalBinding{};
			gNormalBinding.binding = TO_API_FORM(SSAORawBinding::GNormalTex);
			gNormalBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			gNormalBinding.descriptorCount = 1;
			gNormalBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding gDepthBinding{};
			gDepthBinding.binding = TO_API_FORM(SSAORawBinding::GDepthTex);
			gDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			gDepthBinding.descriptorCount = 1;
			gDepthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding noiseBinding{};
			noiseBinding.binding = TO_API_FORM(SSAORawBinding::NoiseTex);
			noiseBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			noiseBinding.descriptorCount = 1;
			noiseBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			ssaoRawDescriptorSets_[i].createLayout({
				uboBinding,
				uboSamplesBinding,
				gNormalBinding, 
				gDepthBinding, 
				noiseBinding
				});

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize uboSamplesPool{};
			uboSamplesPool.type = vk::DescriptorType::eUniformBuffer;
			uboSamplesPool.descriptorCount = 1;

			vk::DescriptorPoolSize gNormalPool{};
			gNormalPool.type = vk::DescriptorType::eCombinedImageSampler;
			gNormalPool.descriptorCount = 1;

			vk::DescriptorPoolSize gDepthPool{};
			gDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
			gDepthPool.descriptorCount = 1;

			vk::DescriptorPoolSize noisePool{};
			noisePool.type = vk::DescriptorType::eCombinedImageSampler;
			noisePool.descriptorCount = 1;

			ssaoRawDescriptorSets_[i].createPool({
				uboPool,
				uboSamplesPool,
				gNormalPool, 
				gDepthPool, 
				noisePool
				});
			ssaoRawDescriptorSets_[i].allocate();

			ssaoRawDescriptorSets_[i].setDebugName(
				"SSAOPassVk-Raw::DescriptorSet frame " + std::to_string(i)
			);
		}


		// BLUR DS
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(SSAOBlurBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding ssaoRawBinding{};
			ssaoRawBinding.binding = TO_API_FORM(SSAOBlurBinding::SSAORawTex);
			ssaoRawBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			ssaoRawBinding.descriptorCount = 1;
			ssaoRawBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			ssaoBlurDescriptorSets_[i].createLayout({
				uboBinding, 
				ssaoRawBinding
				});

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize ssaoRawPool{};
			ssaoRawPool.type = vk::DescriptorType::eCombinedImageSampler;
			ssaoRawPool.descriptorCount = 1;

			ssaoBlurDescriptorSets_[i].createPool({
				uboPool, 
				ssaoRawPool
				});
			ssaoBlurDescriptorSets_[i].allocate();

			ssaoBlurDescriptorSets_[i].setDebugName(
				"SSAOPassVk-Blur::DescriptorSet frame " + std::to_string(i)
			);
		}
	} // end for
} // end of createDescriptorSets()

void SSAOPassVk::createPipelines()
{
	// RAW PIPELINE
	{
		GraphicsPipelineDescVk desc{};
		desc.vertShader = ssaoRawShader_->vertShader();
		desc.fragShader = ssaoRawShader_->fragShader();

		desc.setLayouts = { 
			ssaoRawDescriptorSets_[0].getLayout()
		};

		desc.colorFormat = singleChannelFormat_;
		desc.depthFormat = vk::Format::eUndefined;

		desc.cullMode = vk::CullModeFlagBits::eNone;
		desc.depthTestEnable = false;
		desc.depthWriteEnable = false;

		ssaoRawPipeline_.create(desc);

		ssaoRawPipeline_.setDebugName("SSAOPassVk-Raw::Pipeline");
	}

	// BLUR PIPELINE
	{
		GraphicsPipelineDescVk desc{};
		desc.vertShader = ssaoBlurShader_->vertShader();
		desc.fragShader = ssaoBlurShader_->fragShader();

		desc.setLayouts = { ssaoBlurDescriptorSets_[0].getLayout()};

		desc.colorFormat = singleChannelFormat_;
		desc.depthFormat = vk::Format::eUndefined;

		desc.cullMode = vk::CullModeFlagBits::eNone;
		desc.depthTestEnable = false;
		desc.depthWriteEnable = false;

		ssaoBlurPipeline_.create(desc);

		ssaoBlurPipeline_.setDebugName("SSAOPassVk-Blur::Pipeline");
	}
} // end of createPipelines()

void SSAOPassVk::createNoiseTexture()
{
	using namespace SSAO_Constants;

	std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	std::vector<glm::vec4> noise;
	noise.reserve(K_NOISE_SIZE * K_NOISE_SIZE);

	for (int i = 0; i < K_NOISE_SIZE * K_NOISE_SIZE; ++i)
	{
		glm::vec3 n(dist(rng), dist(rng), 0.0f);
		noise.emplace_back(n, 1.0f);
	} // end for

	// create GPU image
	ssaoNoiseImage_.createImage(
		K_NOISE_SIZE,
		K_NOISE_SIZE,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		noiseFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	ssaoNoiseImage_.createImageView(
		noiseFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	ssaoNoiseImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eRepeat,
		false
	);

	ssaoNoiseImage_.setDebugName("SSAOPassVk-NoiseImage");

	// staging upload
	vk::DeviceSize size = static_cast<vk::DeviceSize>(noise.size() * sizeof(glm::vec4));

	BufferVk staging(vk_);
	staging.create(
		size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	staging.upload(noise.data(), size);

	// copy into image
	VkUtils::TransitionImageLayoutImmediate(
		vk_,
		ssaoNoiseImage_.image(),
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eTransferDstOptimal,
		1,
		1
	);

	VkUtils::CopyBufferToImageImmediate(
		vk_,
		staging.getBuffer(),
		ssaoNoiseImage_.image(),
		K_NOISE_SIZE,
		K_NOISE_SIZE,
		1
	);

	VkUtils::TransitionImageLayoutImmediate(
		vk_,
		ssaoNoiseImage_.image(),
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eTransferDstOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		1,
		1
	);
} // end of createNoise()

void SSAOPassVk::createKernel()
{
	using namespace SSAO_Constants;

	std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist01{ 0.0f,1.0f };

	for (int i = 0; i < MAX_KERNEL_SIZE; ++i)
	{
		// hemisphere around +z (tangent space)
		glm::vec4 s{
			dist01(rng) * 2.0f - 1.0f,
			dist01(rng) * 2.0f - 1.0f,
			dist01(rng),
			0.0f
		};
		s = glm::normalize(s);
		s *= dist01(rng);

		// bias samples toward the origin
		float scale = static_cast<float>(i) / static_cast<float>(KERNEL_SIZE);
		scale = 0.1f + (scale * scale) * (1.0f - 0.1f);
		s *= scale;

		samples_[i] = s;
	} // end for
} // end of createKernel()