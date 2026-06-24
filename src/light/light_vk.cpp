#include "light_vk.h"

#include "bindings.h"

#include "frame_context_vk.h"

#include "vulkan_main.h"
#include "shader_vk.h"

#include <vulkan/vulkan.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <array>
#include <cstdint>

using namespace Light_Constants;

//--- PUBLIC ---//
LightVk::LightVk(VulkanMain& vk)
	: vk_(vk),
	pipeline_(vk),
	pipelineOffscreen_(vk)
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

LightVk::~LightVk() = default;

void LightVk::init()
{
	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(), 
		"light/light.vert.spv", 
		"light/light.frag.spv"
	);

	createUBOs();
	createDescriptorSets();
	createPipeline();
} // end of init()

void LightVk::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& proj
)
{
	if (!descriptorSets_[frameVk->frameIndex].valid() ||
		!uboBuffers_[frameVk->frameIndex].valid() ||
		!pipeline_.valid()) return;

	vk::CommandBuffer cmd = frameVk->cmd;

	cmd.beginDebugUtilsLabelEXT({ "LightVk-Default::cmd" });

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());

	LightUBO ubo{};
	ubo.u_invViewProj = glm::inverse(proj * view);
	ubo.u_viewProj = proj * view;
	ubo.u_camPos = camPos_;
	ubo.u_sunDistance = SUN_DISTANCE;
	ubo.u_lightPos = glm::vec4(position_, 1.0f);
	ubo.u_lightVisualColor = visualColor_;
	ubo.u_sunRadius = SUN_SCALE / 2.0f;

	uboBuffers_[frameVk->frameIndex].upload(&ubo, sizeof(LightUBO));

	vk::DescriptorSet descSet = descriptorSets_[frameVk->frameIndex].getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(3, 1, 0, 0);

	cmd.endDebugUtilsLabelEXT();
} // end of render()

void LightVk::renderOffscreen(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& proj,
	uint32_t width,
	uint32_t height
)
{
	if (!descriptorSetsOffscreen_[frameVk->frameIndex].valid() ||
		!uboBuffersOffscreen_[frameVk->frameIndex].valid() ||
		!pipelineOffscreen_.valid()) return;

	vk::CommandBuffer cmd = frameVk->cmd;

	cmd.beginDebugUtilsLabelEXT({ "LightVk-Offscreen::cmd" });

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

	glm::mat4 model = glm::translate(glm::mat4(1.0f), position_);
	model = glm::scale(model, glm::vec3(SUN_SCALE));

	LightUBO ubo{};
	ubo.u_invViewProj = glm::inverse(proj * view);
	ubo.u_viewProj = proj * view;
	ubo.u_camPos = camPos_;
	ubo.u_sunDistance = SUN_DISTANCE;
	ubo.u_lightPos = glm::vec4(position_, 1.0f);
	ubo.u_lightVisualColor = visualColor_;
	ubo.u_sunRadius = SUN_SCALE / 2.0f;

	uboBuffersOffscreen_[frameVk->frameIndex].upload(&ubo, sizeof(LightUBO));

	vk::DescriptorSet descSet = descriptorSetsOffscreen_[frameVk->frameIndex].getSet();
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipelineOffscreen_.getLayout(),
		0,
		1, &descSet,
		0, nullptr
	);

	cmd.draw(3, 1, 0, 0);

	cmd.endDebugUtilsLabelEXT();
} // end of renderOffscreen()

void LightVk::updateLight(
	float time,
	const glm::vec3& camPos,
	bool paused
)
{	
	camPos_ = camPos;

	if (firstUpdate_)
	{
		lastTime_ = time;
		firstUpdate_ = false;
	}

	float dt = time - lastTime_;
	lastTime_ = time;

	if (!paused)
	{
		sunTime_ += dt * speed_;
	}

	// update direction
	float cycle = sunTime_ * glm::two_pi<float>();

	float azimuth = cycle;
	float elevation = glm::sin(cycle) * glm::radians(75.0f);

	glm::vec3 sunDir = glm::normalize(glm::vec3(
		glm::cos(elevation) * glm::cos(azimuth),
		glm::sin(elevation),
		glm::cos(elevation) * glm::sin(azimuth)
	));
	setDirection(sunDir);

	// update position
	glm::vec3 sunVisualPos = camPos - direction_ * SUN_DISTANCE;
	setPosition(sunVisualPos);

	// update visual color
	float sunHeight = glm::max(-direction_.y, 0.0f);
	float tColor = glm::smoothstep(0.0f, 0.35f, sunHeight);

	glm::vec3 sunsetColor = glm::vec3(
		INIT_VISUAL_COLOR.r,
		INIT_VISUAL_COLOR.g / 2.0f,
		INIT_VISUAL_COLOR.b
	);

	glm::vec3 noonColor = INIT_VISUAL_COLOR;

	visualColor_ = glm::mix(
		sunsetColor,
		noonColor,
		tColor
	);
} // end of updateLight()


//--- PRIVATE ---//
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
			"LightVk-Default::DescriptorSet frame " + std::to_string(i)
		);

		descriptorSetsOffscreen_[i].createSingleUniformBuffer(
			TO_API_FORM(LightBinding::UBO),
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			uboBuffersOffscreen_[i].getBuffer(),
			sizeof(LightUBO)
		);

		descriptorSetsOffscreen_[i].setDebugName(
			"LightVk-Offscreen::DescriptorSet frame " + std::to_string(i)
		);
	} // end for
} // end of createDescriptorSets()

void LightVk::createPipeline()
{
	// normal pipeline
	GraphicsPipelineDescVk desc{};
	desc.vertShader = shader_->vertShader();
	desc.fragShader = shader_->fragShader();

	desc.setLayouts = { descriptorSets_[0].getLayout()};

	desc.colorFormat = vk::Format::eR16G16B16A16Sfloat;
	desc.depthFormat = vk::Format::eD32Sfloat;

	desc.cullMode = vk::CullModeFlagBits::eBack;
	desc.frontFace = vk::FrontFace::eClockwise;
	desc.depthTestEnable = true;
	desc.depthWriteEnable = true;
	desc.depthCompareOp = vk::CompareOp::eLessOrEqual;

	pipeline_.create(desc);

	pipeline_.setDebugName("LightVk-Default::Pipeline");

	// offscreen pipeline
	pipelineOffscreen_.create(desc);

	pipelineOffscreen_.setDebugName("LightVk-Offscreen::Pipeline");
} // end of createPipeline()
