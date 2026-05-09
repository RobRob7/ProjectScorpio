#include "light_vk.h"

#include "bindings.h"

#include "frame_context_vk.h"

#include "vulkan_main.h"
#include "shader_vk.h"

#include <vulkan/vulkan.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <array>
#include <cassert>
#include <cstdint>

using namespace Light_Constants;

//--- PUBLIC ---//
LightVk::LightVk(
	VulkanMain& vk,
	const glm::vec3& pos, 
	const glm::vec3& dir,
	const glm::vec3& color
)
	: vk_(vk),
	vertexBuffer_(vk),
	pipeline_(vk),
	pipelineOffscreen_(vk),
	position_(pos)
{
	setDirection(dir);
	setColor(color);

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

LightVk::~LightVk() = default;

void LightVk::init()
{
	shader_ = std::make_unique<ShaderModuleVk>(vk_.getDevice(), "light/light.vert.spv", "light/light.frag.spv");

	createVertexBuffer();
	createUBOs();
	createDescriptorSets();
	createPipeline();
} // end of init()

void LightVk::render(
	const FrameContext* frame,
	const glm::mat4& view,
	const glm::mat4& proj
)
{
	assert(frame->cmd && "Must be valid Vulkan frame context!");

	if (!descriptorSets_[frame->frameIndex].valid() ||
		!uboBuffers_[frame->frameIndex].valid() ||
		!vertexBuffer_.valid() || 
		!pipeline_.valid()) return;

	vk::CommandBuffer cmd = frame->cmd;

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());

	vk::Buffer vertBuffer = vertexBuffer_.getBuffer();
	vk::DeviceSize offset = 0;
	cmd.bindVertexBuffers(0, 1, &vertBuffer, &offset);

	glm::mat4 model = glm::translate(glm::mat4(1.0f), position_);

	LightUBO ubo{};
	ubo.model = model;
	ubo.view = view;
	ubo.proj = proj;
	ubo.color = glm::vec4(color_, 1.0f);

	uboBuffers_[frame->frameIndex].upload(&ubo, sizeof(LightUBO));

	vk::DescriptorSet descSet = descriptorSets_[frame->frameIndex].getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(vertexCount_, 1, 0, 0);
} // end of render()

void LightVk::renderOffscreen(
	const FrameContext* frame,
	const glm::mat4& view,
	const glm::mat4& proj,
	const glm::vec3& position,
	uint32_t width,
	uint32_t height
)
{
	assert(frame->cmd && "Must be valid Vulkan frame context!");

	if (!descriptorSetsOffscreen_[frame->frameIndex].valid() ||
		!uboBuffersOffscreen_[frame->frameIndex].valid() ||
		!vertexBuffer_.valid() || 
		!pipelineOffscreen_.valid()) return;

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

	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);

	LightUBO ubo{};
	ubo.model = model;
	ubo.view = view;
	ubo.proj = proj;
	ubo.color = glm::vec4(color_, 1.0f);

	uboBuffersOffscreen_[frame->frameIndex].upload(&ubo, sizeof(LightUBO));

	vk::DescriptorSet descSet = descriptorSetsOffscreen_[frame->frameIndex].getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipelineOffscreen_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(vertexCount_, 1, 0, 0);
} // end of renderOffscreen()

void LightVk::updateLightDirection(float time)
{	
	if (speed_ <= 0.0f)
	{
		return;
	}

	float t = time * speed_;
	float cycle = t * glm::two_pi<float>();

	float azimuth = cycle;
	float elevation = glm::sin(cycle) * glm::radians(75.0f);

	glm::vec3 sunDir = glm::normalize(glm::vec3(
		glm::cos(elevation) * glm::cos(azimuth),
		glm::sin(elevation),
		glm::cos(elevation) * glm::sin(azimuth)
	));

	setDirection(sunDir);
} // end of updateLightDirection()


//--- PRIVATE ---//
void LightVk::createVertexBuffer()
{
	vertexCount_ = static_cast<uint32_t>(CUBE_VERTICES.size() / 3);

	const vk::DeviceSize bufferSize = sizeof(float) * CUBE_VERTICES.size();
	vertexBuffer_.create(
		bufferSize,
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	vertexBuffer_.upload(CUBE_VERTICES.data(), bufferSize);
} // end of createVertexBuffer()

void LightVk::createUBOs()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_[i].create(
			sizeof(LightUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		uboBuffersOffscreen_[i].create(
			sizeof(LightUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createUBOs()

void LightVk::createDescriptorSets()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		vk::DescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = TO_API_FORM(LightBinding::UBO);
		uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

		descriptorSets_[i].createSingleUniformBuffer(
			TO_API_FORM(LightBinding::UBO),
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			uboBuffers_[i].getBuffer(),
			sizeof(LightUBO)
		);

		descriptorSets_[i].setDebugName(
			"LightVk::descriptorSets_ frame " + std::to_string(i)
		);

		descriptorSetsOffscreen_[i].createSingleUniformBuffer(
			TO_API_FORM(LightBinding::UBO),
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			uboBuffersOffscreen_[i].getBuffer(),
			sizeof(LightUBO)
		);

		descriptorSetsOffscreen_[i].setDebugName(
			"LightVk::descriptorSetsOffscreen_ frame " + std::to_string(i)
		);
	} // end for
} // end of createDescriptorSets()

void LightVk::createPipeline()
{
	// normal pipeline
	vk::VertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(VertexLight);
	binding.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription attr{};
	attr.location = 0;
	attr.binding = 0;
	attr.format = vk::Format::eR32G32B32Sfloat;
	attr.offset = offsetof(VertexLight, pos);

	GraphicsPipelineDescVk desc{};
	desc.vertShader = shader_->vertShader();
	desc.fragShader = shader_->fragShader();

	desc.setLayouts = { descriptorSets_[0].getLayout()};

	desc.vertexBinding = binding;
	desc.vertexAttributes = { attr };

	desc.colorFormat = vk::Format::eR16G16B16A16Sfloat;
	desc.depthFormat = vk::Format::eD32Sfloat;

	desc.cullMode = vk::CullModeFlagBits::eBack;
	desc.frontFace = vk::FrontFace::eClockwise;
	desc.depthTestEnable = true;
	desc.depthWriteEnable = true;
	desc.depthCompareOp = vk::CompareOp::eLessOrEqual;

	pipeline_.create(desc);


	// offscreen pipeline
	pipelineOffscreen_.create(desc);
} // end of createPipeline()
