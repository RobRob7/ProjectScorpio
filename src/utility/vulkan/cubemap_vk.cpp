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
	uboBuffer_(vk),
	uboBufferOffscreen_(vk),
	descriptorSet_(vk),
	descriptorSetOffscreen_(vk),
	pipeline_(vk),
	pipelineOffscreen_(vk),
	faces_(textures),
	cubemapTextureNight_(vk),
	cubemapTextureDay_(vk)
{
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
	createUBO();

	cubemapTextureNight_.loadFromFiles(faces_);
	cubemapTextureDay_.loadFromFiles(DAY_FACES);

	createDescriptorSet();

	createPipeline();
} // end of init()

void CubemapVk::render(
	const FrameContext* frame,
	const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& sunDir,
	const float time
)
{
	assert(frame->cmd && "Must be valid Vulkan frame context!");

	if (!descriptorSet_.valid() || !uboBuffer_.valid() || !vertexBuffer_.valid() || !pipeline_.valid()) return;

	vk::CommandBuffer cmd = frame->cmd;

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

	uboBuffer_.upload(&ubo, sizeof(CubemapUBO));

	vk::DescriptorSet descSet = descriptorSet_.getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(vertexCount_, 1, 0, 0);
} // end of render()

void CubemapVk::renderOffscreen(
	const FrameContext* frame,
	const glm::mat4& view,
	const glm::mat4& projection,
	uint32_t width,
	uint32_t height,
	const glm::vec3& sunDir,
	const float time
)
{
	assert(frame->cmd && "Must be valid Vulkan frame context!");

	if (!descriptorSetOffscreen_.valid() || !uboBufferOffscreen_.valid() || !vertexBuffer_.valid() || !pipelineOffscreen_.valid()) return;

	vk::CommandBuffer cmd = frame->cmd;

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

	uboBufferOffscreen_.upload(&ubo, sizeof(CubemapUBO));

	vk::DescriptorSet descSet = descriptorSetOffscreen_.getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipelineOffscreen_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(vertexCount_, 1, 0, 0);
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

void CubemapVk::createUBO()
{
	uboBuffer_.create(
		sizeof(CubemapUBO),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	uboBufferOffscreen_.create(
		sizeof(CubemapUBO),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
} // end of createUBO()

void CubemapVk::createDescriptorSet()
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

	descriptorSet_.createLayout({ uboBinding, nightTexBinding, dayTexBinding });

	vk::DescriptorPoolSize uboPool{};
	uboPool.type = vk::DescriptorType::eUniformBuffer;
	uboPool.descriptorCount = 1;

	vk::DescriptorPoolSize nightTexPool{};
	nightTexPool.type = vk::DescriptorType::eCombinedImageSampler;
	nightTexPool.descriptorCount = 1;

	vk::DescriptorPoolSize dayTexPool{};
	dayTexPool.type = vk::DescriptorType::eCombinedImageSampler;
	dayTexPool.descriptorCount = 1;

	descriptorSet_.createPool({ uboPool, nightTexPool, dayTexPool }, 1);
	descriptorSet_.allocate();

	descriptorSet_.writeUniformBuffer(
		TO_API_FORM(CubemapBinding::UBO),
		uboBuffer_.getBuffer(),
		sizeof(CubemapUBO)
	);

	descriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(CubemapBinding::NightSkyboxTex),
		cubemapTextureNight_.view(),
		cubemapTextureNight_.sampler()
	);

	descriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(CubemapBinding::DaySkyboxTex),
		cubemapTextureDay_.view(),
		cubemapTextureDay_.sampler()
	);

	// offscreen descriptor set
	descriptorSetOffscreen_.createLayout({ uboBinding, nightTexBinding, dayTexBinding, });

	descriptorSetOffscreen_.createPool({ uboPool, nightTexPool, dayTexPool }, 1);
	descriptorSetOffscreen_.allocate();

	descriptorSetOffscreen_.writeUniformBuffer(
		TO_API_FORM(CubemapBinding::UBO),
		uboBufferOffscreen_.getBuffer(),
		sizeof(CubemapUBO)
	);

	descriptorSetOffscreen_.writeCombinedImageSampler(
		TO_API_FORM(CubemapBinding::NightSkyboxTex),
		cubemapTextureNight_.view(),
		cubemapTextureNight_.sampler()
	);

	descriptorSetOffscreen_.writeCombinedImageSampler(
		TO_API_FORM(CubemapBinding::DaySkyboxTex),
		cubemapTextureDay_.view(),
		cubemapTextureDay_.sampler()
	);
} // end of createDescriptorSet()

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

	desc.setLayouts = { descriptorSet_.getLayout() };

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

	// offscreen pipeline
	desc.colorFormat = vk::Format::eR16G16B16A16Sfloat;
	desc.depthFormat = vk::Format::eD32Sfloat;

	pipelineOffscreen_.create(desc);
} // end of createPipeline()