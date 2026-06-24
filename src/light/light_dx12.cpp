#include "light_dx12.h"

#include "bindings.h"

#include "frame_context_dx12.h"

#include "dx12_main.h"
#include "shader_dx12.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <array>
#include <cstdint>
#include <stdexcept>

using namespace Light_Constants;

//--- PUBLIC ---//
LightDX12::LightDX12(DX12Main& dx)
	: dx_(&dx),
	pipeline_(dx),
	pipelineOffscreen_(dx)
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	uboBuffers_.reserve(frames);
	uboBuffersOffscreen_.reserve(frames);

	descriptorSets_.reserve(frames);
	descriptorSetsOffscreen_.reserve(frames);

	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_.emplace_back(dx);
		uboBuffersOffscreen_.emplace_back(dx);

		descriptorSets_.emplace_back(dx);
		descriptorSetsOffscreen_.emplace_back(dx);
	} // end for
} // end of constructor

LightDX12::~LightDX12() = default;

void LightDX12::init()
{
	shader_ = std::make_unique<ShaderDX12>(
		"hlsl/light/light.vert.cso", 
		"hlsl/light/light.frag.cso"
	);

	createUBOs();
	createDescriptorSets();
	createPipeline();
} // end of init()

void LightDX12::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& proj
)
{
	const FrameContextDX12& frame = *frameDX12;

	if (!descriptorSets_[frame.frameIndex].valid() ||
		!uboBuffers_[frame.frameIndex].valid() ||
		!pipeline_.valid()) return;

	ID3D12GraphicsCommandList* cmd = frame.cmd;

	LightUBO ubo{};
	ubo.u_invViewProj = glm::inverse(proj * view);
	ubo.u_viewProj = proj * view;
	ubo.u_camPos = camPos_;
	ubo.u_sunDistance = SUN_DISTANCE;
	ubo.u_lightPos = glm::vec4(position_, 1.0f);
	ubo.u_lightVisualColor = visualColor_;
	ubo.u_sunRadius = SUN_SCALE / 2.0f;

	uboBuffers_[frame.frameIndex].upload(&ubo, sizeof(LightUBO));

	ID3D12DescriptorHeap* heaps[] =
	{
		descriptorSets_[frame.frameIndex].getDescriptorHeap()
	};

	cmd->SetDescriptorHeaps(1, heaps);
	cmd->SetGraphicsRootSignature(pipeline_.getRootSignature());
	cmd->SetPipelineState(pipeline_.getPipeline());

	cmd->SetGraphicsRootDescriptorTable(
		0,
		descriptorSets_[frame.frameIndex].getTableGPUHandle()
	);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
} // end of render()

void LightDX12::renderOffscreen(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& proj,
	uint32_t width,
	uint32_t height
)
{
	const FrameContextDX12& frame = *frameDX12;

	if (!descriptorSetsOffscreen_[frame.frameIndex].valid() ||
		!uboBuffersOffscreen_[frame.frameIndex].valid() ||
		!pipelineOffscreen_.valid()) return;

	ID3D12GraphicsCommandList* cmd = frame.cmd;

	LightUBO ubo{};
	ubo.u_invViewProj = glm::inverse(proj * view);
	ubo.u_viewProj = proj * view;
	ubo.u_camPos = camPos_;
	ubo.u_sunDistance = SUN_DISTANCE;
	ubo.u_lightPos = glm::vec4(position_, 1.0f);
	ubo.u_lightVisualColor = visualColor_;
	ubo.u_sunRadius = SUN_SCALE / 2.0f;

	uboBuffersOffscreen_[frame.frameIndex].upload(&ubo, sizeof(LightUBO));

	ID3D12DescriptorHeap* heaps[] =
	{
		descriptorSetsOffscreen_[frame.frameIndex].getDescriptorHeap()
	};

	cmd->SetDescriptorHeaps(1, heaps);
	cmd->SetGraphicsRootSignature(pipeline_.getRootSignature());
	cmd->SetPipelineState(pipeline_.getPipeline());

	cmd->SetGraphicsRootDescriptorTable(
		0,
		descriptorSetsOffscreen_[frame.frameIndex].getTableGPUHandle()
	);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(3, 1, 0, 0);
} // end of renderOffscreen()

void LightDX12::updateLight(
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
void LightDX12::createUBOs()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_[i].create(
			sizeof(LightUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);

		uboBuffersOffscreen_[i].create(
			sizeof(LightUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);
	} // end for
} // end of createUBOs()

void LightDX12::createDescriptorSets()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		// default
		{
			DescriptorBindingDX12 uboBinding{
					.binding = TO_API_FORM(LightBinding::UBO),
					.type = DescriptorTypeDX12::UniformBuffer,
					.count = 1,
					.visibility = D3D12_SHADER_VISIBILITY_ALL
			};

			descriptorSets_[i].createLayout({
				uboBinding
			});

			descriptorSets_[i].createPool(1);
			descriptorSets_[i].allocate();

			descriptorSets_[i].setDebugName(
				L"LightDX12-Default::DescriptorSet frame " + std::to_wstring(i)
			);

			descriptorSets_[i].writeUniformBuffer(
				TO_API_FORM(LightBinding::UBO),
				uboBuffers_[i],
				sizeof(LightUBO)
			);
		}

		// offscreen
		{
			DescriptorBindingDX12 uboBinding{
					.binding = TO_API_FORM(LightBinding::UBO),
					.type = DescriptorTypeDX12::UniformBuffer,
					.count = 1,
					.visibility = D3D12_SHADER_VISIBILITY_ALL
			};

			descriptorSetsOffscreen_[i].createLayout({
				uboBinding
			});

			descriptorSetsOffscreen_[i].createPool(1);
			descriptorSetsOffscreen_[i].allocate();

			descriptorSetsOffscreen_[i].setDebugName(
				L"LightDX12-Offscreen::DescriptorSet frame " + std::to_wstring(i)
			);

			descriptorSetsOffscreen_[i].writeUniformBuffer(
				TO_API_FORM(LightBinding::UBO),
				uboBuffersOffscreen_[i],
				sizeof(LightUBO)
			);
		}
	} // end for
} // end of createDescriptorSets()

void LightDX12::createPipeline()
{
	// normal pipeline
	GraphicsPipelineDescDX12 desc{
			.vertShader = shader_->vertShader(),
			.fragShader = shader_->fragShader(),

			.rootSignature = descriptorSets_[0].getRootSignature(),

			.cullMode = D3D12_CULL_MODE_NONE,
			.frontCCW = FALSE,

			.depthTestEnable = TRUE,
			.depthWriteEnable = TRUE,
			.depthCompareFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,

			.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
			.depthFormat = DXGI_FORMAT_D32_FLOAT
	};
	pipeline_.create(desc);
	pipeline_.setDebugName(L"LightDX12-Default::Pipeline");

	// offscreen pipeline
	pipelineOffscreen_.create(desc);
	pipelineOffscreen_.setDebugName(L"LightDX12-Offscreen::Pipeline");
} // end of createPipeline()
