#include "cubemap_vk.h"

#include "bindings.h"

#include "frame_context_vk.h"

#include "vulkan_main.h"
#include "shader_vk.h"

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <string_view>
#include <cstddef>
#include <cassert>

using namespace Cubemap_Constants;

//--- PUBLIC ---//
CubemapVk::CubemapVk(
	VulkanMain& vk, 
	const std::array<std::string_view, 6>& textures
)
	: vk_(vk),
	vertexBuffer_(vk),
	pipeline_(vk),
	pipelineOffscreen_(vk),
	faces_(textures),
	cubemapTextureNight_(vk),
	cubemapTextureDay_(vk)
{
	uboBuffers_.reserve(vk_.getMaxFramesInFlight());
	uboBuffersOffscreen_.reserve(vk_.getMaxFramesInFlight());

	descriptorSets_.reserve(vk_.getMaxFramesInFlight());
	descriptorSetsOffscreen_.reserve(vk_.getMaxFramesInFlight());

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_.emplace_back(vk_);
		uboBuffersOffscreen_.emplace_back(vk_);

		descriptorSets_.emplace_back(vk_);
		descriptorSetsOffscreen_.emplace_back(vk_);
	} // end for
} // end of constructor
CubemapVk::~CubemapVk() = default;

void CubemapVk::init()
{
	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(), 
		"cubemap/cubemap.vert.spv", 
		"cubemap/cubemap.frag.spv"
	);

	createVertexBuffer();
	createUBOs();

	cubemapTextureNight_.loadFromFiles(faces_);
	cubemapTextureDay_.loadFromFiles(DAY_FACES);

	createDescriptorSets();

	createPipeline();
} // end of init()

void CubemapVk::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& sunDir,
	const float time
)
{
	assert(frameVk->cmd && "Must be valid Vulkan frame context!");

	if (!descriptorSets_[frameVk->frameIndex].valid() || 
		!uboBuffers_[frameVk->frameIndex].valid() || 
		!vertexBuffer_.valid() || 
		!pipeline_.valid()) return;

	vk::CommandBuffer cmd = frameVk->cmd;

	cmd.beginDebugUtilsLabelEXT({ "CubemapVk-Default::cmd" });

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());

	vk::Buffer vertBuffer = vertexBuffer_.getBuffer();
	vk::DeviceSize offset = 0;
	cmd.bindVertexBuffers(0, 1, &vertBuffer, &offset);

	glm::mat4 viewStrippedTranslation = glm::mat4(glm::mat3(view));

	if (time > 0.0f)
	{
		float speed = 0.005f;

		glm::mat4 skyRot = glm::rotate(glm::mat4(1.0f),
			time * speed,
			glm::vec3(0.0f, 1.0f, 0.0f));
		viewStrippedTranslation = viewStrippedTranslation * glm::mat4(glm::mat3(skyRot));
	}

	CubemapUBO ubo{};
	ubo.u_dayNightMix = glm::clamp((sunDir.y + 0.15f) / 0.30f, 0.0f, 1.0f);
	ubo.u_view = viewStrippedTranslation;
	ubo.u_proj = projection;

	uboBuffers_[frameVk->frameIndex].upload(&ubo, sizeof(CubemapUBO));

	vk::DescriptorSet descSet = descriptorSets_[frameVk->frameIndex].getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(vertexCount_, 1, 0, 0);

	cmd.endDebugUtilsLabelEXT();
} // end of render()

void CubemapVk::renderOffscreen(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& projection,
	uint32_t width,
	uint32_t height,
	const glm::vec3& sunDir,
	const float time
)
{
	assert(frameVk->cmd && "Must be valid Vulkan frame context!");

	if (!descriptorSetsOffscreen_[frameVk->frameIndex].valid() ||
		!uboBuffersOffscreen_[frameVk->frameIndex].valid() ||
		!vertexBuffer_.valid() || 
		!pipelineOffscreen_.valid()) return;

	vk::CommandBuffer cmd = frameVk->cmd;

	cmd.beginDebugUtilsLabelEXT({ "CubemapVk-Offscreen::cmd" });

	vk::Viewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = vk::Extent2D{ width, height };

	cmd.setViewport(0, 1, &viewport);
	cmd.setScissor(0, 1, &scissor);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineOffscreen_.getPipeline());

	vk::Buffer vertBuffer = vertexBuffer_.getBuffer();
	vk::DeviceSize offset = 0;
	cmd.bindVertexBuffers(0, 1, &vertBuffer, &offset);

	glm::mat4 viewStrippedTranslation = glm::mat4(glm::mat3(view));

	if (time > 0.0f)
	{
		float speed = 0.005f;

		glm::mat4 skyRot = glm::rotate(glm::mat4(1.0f),
			time * speed,
			glm::vec3(0.0f, 1.0f, 0.0f));
		viewStrippedTranslation = viewStrippedTranslation * glm::mat4(glm::mat3(skyRot));
	}

	CubemapUBO ubo{};
	ubo.u_dayNightMix = glm::clamp((sunDir.y + 0.15f) / 0.30f, 0.0f, 1.0f);
	ubo.u_view = viewStrippedTranslation;
	ubo.u_proj = projection;

	uboBuffersOffscreen_[frameVk->frameIndex].upload(&ubo, sizeof(CubemapUBO));

	vk::DescriptorSet descSet = descriptorSetsOffscreen_[frameVk->frameIndex].getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipelineOffscreen_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(vertexCount_, 1, 0, 0);

	cmd.endDebugUtilsLabelEXT();
} // end of renderOffscreen()


//--- PRIVATE ---//
void CubemapVk::createVertexBuffer()
{
	vertexCount_ = static_cast<uint32_t>(SKYBOX_VERTICES.size() / 3);

	const vk::DeviceSize bufferSize = sizeof(float) * SKYBOX_VERTICES.size();
	vertexBuffer_.create(
		bufferSize,
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	vertexBuffer_.upload(SKYBOX_VERTICES.data(), bufferSize);
} // end of createVertexBuffer()

void CubemapVk::createUBOs()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_[i].create(
			sizeof(CubemapUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		uboBuffersOffscreen_[i].create(
			sizeof(CubemapUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createUBOs()

void CubemapVk::createDescriptorSets()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		// normal descriptor set
		vk::DescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = TO_API_FORM(CubemapBinding::UBO);
		uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding nightTexBinding{};
		nightTexBinding.binding = TO_API_FORM(CubemapBinding::NightSkyboxTex);
		nightTexBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		nightTexBinding.descriptorCount = 1;
		nightTexBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding dayTexBinding{};
		dayTexBinding.binding = TO_API_FORM(CubemapBinding::DaySkyboxTex);
		dayTexBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		dayTexBinding.descriptorCount = 1;
		dayTexBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		descriptorSets_[i].createLayout({
			uboBinding, 
			nightTexBinding, 
			dayTexBinding
			});

		vk::DescriptorPoolSize uboPool{};
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize nightTexPool{};
		nightTexPool.type = vk::DescriptorType::eCombinedImageSampler;
		nightTexPool.descriptorCount = 1;

		vk::DescriptorPoolSize dayTexPool{};
		dayTexPool.type = vk::DescriptorType::eCombinedImageSampler;
		dayTexPool.descriptorCount = 1;

		descriptorSets_[i].createPool({
			uboPool, 
			nightTexPool, 
			dayTexPool
			});
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			"CubemapVk-Default::DescriptorSet frame " + std::to_string(i)
		);

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(CubemapBinding::UBO),
			uboBuffers_[i].getBuffer(),
			sizeof(CubemapUBO)
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(CubemapBinding::NightSkyboxTex),
			cubemapTextureNight_.view(),
			cubemapTextureNight_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(CubemapBinding::DaySkyboxTex),
			cubemapTextureDay_.view(),
			cubemapTextureDay_.sampler()
		);

		// offscreen descriptor set
		descriptorSetsOffscreen_[i].createLayout({
			uboBinding, 
			nightTexBinding, 
			dayTexBinding
			});

		descriptorSetsOffscreen_[i].createPool({
			uboPool, 
			nightTexPool, 
			dayTexPool
			});
		descriptorSetsOffscreen_[i].allocate();

		descriptorSetsOffscreen_[i].setDebugName(
			"CubemapVk-Offscreen::DescriptorSet frame " + std::to_string(i)
		);

		descriptorSetsOffscreen_[i].writeUniformBuffer(
			TO_API_FORM(CubemapBinding::UBO),
			uboBuffersOffscreen_[i].getBuffer(),
			sizeof(CubemapUBO)
		);

		descriptorSetsOffscreen_[i].writeCombinedImageSampler(
			TO_API_FORM(CubemapBinding::NightSkyboxTex),
			cubemapTextureNight_.view(),
			cubemapTextureNight_.sampler()
		);

		descriptorSetsOffscreen_[i].writeCombinedImageSampler(
			TO_API_FORM(CubemapBinding::DaySkyboxTex),
			cubemapTextureDay_.view(),
			cubemapTextureDay_.sampler()
		);
	} // end for
} // end of createDescriptorSets()

void CubemapVk::createPipeline()
{
	// normal pipeline
	vk::VertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(VertexCubemap);
	binding.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription attr{};
	attr.location = 0;
	attr.binding = 0;
	attr.format = vk::Format::eR32G32B32Sfloat;
	attr.offset = offsetof(VertexCubemap, pos);

	GraphicsPipelineDescVk desc{};
	desc.vertShader = shader_->vertShader();
	desc.fragShader = shader_->fragShader();

	desc.setLayouts = { descriptorSets_[0].getLayout()};

	desc.vertexBinding = binding;
	desc.vertexAttributes = { attr };

	desc.colorFormat = vk::Format::eR16G16B16A16Sfloat;
	desc.depthFormat = vk::Format::eD32Sfloat;

	desc.cullMode = vk::CullModeFlagBits::eFront;
	desc.frontFace = vk::FrontFace::eClockwise;
	desc.depthTestEnable = true;
	desc.depthWriteEnable = false;
	desc.depthCompareOp = vk::CompareOp::eLessOrEqual;

	pipeline_.create(desc);

	pipeline_.setDebugName("CubemapVk-Default::Pipeline");

	// offscreen pipeline
	desc.colorFormat = vk::Format::eR16G16B16A16Sfloat;
	desc.depthFormat = vk::Format::eD32Sfloat;

	pipelineOffscreen_.create(desc);

	pipelineOffscreen_.setDebugName("CubemapVk-Offscreen::Pipeline");
} // end of createPipeline()