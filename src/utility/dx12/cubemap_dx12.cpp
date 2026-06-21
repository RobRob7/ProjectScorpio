#include "cubemap_dx12.h"

#include "bindings.h"

#include "frame_context_dx12.h"

#include "dx12_main.h"
#include "shader_dx12.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <string_view>
#include <cstddef>

using namespace Cubemap_Constants;

//--- PUBLIC ---//
CubemapDX12::CubemapDX12(
	DX12Main& dx,
	const std::array<std::string_view, 6>& textures
)
	: dx_(&dx),
	vertexBuffer_(*dx_),
	pipeline_(*dx_),
	pipelineOffscreen_(*dx_),
	faces_(textures),
	cubemapTextureNight_(*dx_),
	cubemapTextureDay_(*dx_)
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	uboBuffers_.reserve(frames);
	uboBuffersOffscreen_.reserve(frames);

	descriptorSets_.reserve(frames);
	descriptorSetsOffscreen_.reserve(frames);

	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_.emplace_back(*dx_);
		uboBuffersOffscreen_.emplace_back(*dx_);

		descriptorSets_.emplace_back(*dx_);
		descriptorSetsOffscreen_.emplace_back(*dx_);
	} // end for
} // end of constructor
CubemapDX12::~CubemapDX12() = default;

void CubemapDX12::init()
{
	shader_ = std::make_unique<ShaderDX12>(
		"hlsl/cubemap/cubemap.vert.cso",
		"hlsl/cubemap/cubemap.frag.cso"
	);

	createVertexBuffer();
	createUBOs();

	cubemapTextureNight_.loadFromFiles(faces_);
	cubemapTextureDay_.loadFromFiles(DAY_FACES);

	createDescriptorSets();
	createPipeline();
} // end of init()

void CubemapDX12::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& sunDir,
	const float time
)
{
	if (!frameDX12 || !frameDX12->cmd) return;

	const uint32_t frameIndex = frameDX12->frameIndex;


	if (!descriptorSets_[frameDX12->frameIndex].valid() ||
		!uboBuffers_[frameDX12->frameIndex].valid() ||
		!vertexBuffer_.valid() ||
		!pipeline_.valid())
	{
		return;
	}

	ID3D12GraphicsCommandList* cmd = frameDX12->cmd;

	glm::mat4 viewStrippedTranslation = glm::mat4(glm::mat3(view));

	if (time > 0.0f)
	{
		float speed = 0.005f;

		glm::mat4 skyRot = glm::rotate(glm::mat4(1.0f),
			time * speed,
			glm::vec3(0.0f, 1.0f, 0.0f));

		viewStrippedTranslation = viewStrippedTranslation * glm::mat4(glm::mat3(skyRot));
	}

	CubemapUBO ubo{
		.u_view = viewStrippedTranslation,
		.u_proj = projection,
		.u_dayNightMix = glm::clamp((sunDir.y + 0.15f) / 0.30f, 0.0f, 1.0f)
	};

	uboBuffers_[frameDX12->frameIndex].upload(&ubo, sizeof(ubo));

	ID3D12DescriptorHeap* heaps[] =
	{
		descriptorSets_[frameIndex].getDescriptorHeap()
	};

	cmd->SetDescriptorHeaps(1, heaps);

	cmd->SetGraphicsRootSignature(pipeline_.getRootSignature());

	cmd->SetPipelineState(pipeline_.getPipeline());

	cmd->SetGraphicsRootDescriptorTable(
		0,
		descriptorSets_[frameIndex].getTableGPUHandle()
	);

	D3D12_VERTEX_BUFFER_VIEW vbv{
		.BufferLocation = vertexBuffer_.getGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(vertexBuffer_.size()),
		.StrideInBytes = sizeof(float) * 3
	};

	cmd->IASetVertexBuffers(0, 1, &vbv);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmd->DrawInstanced(
		vertexCount_,
		1,
		0,
		0
	);
} // end of render()

void CubemapDX12::renderOffscreen(
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
	//assert(frame->cmd && "Must be valid Vulkan frame context!");

	//if (!descriptorSetsOffscreen_[frame->frameIndex].valid() ||
	//	!uboBuffersOffscreen_[frame->frameIndex].valid() ||
	//	!vertexBuffer_.valid() || 
	//	!pipelineOffscreen_.valid()) return;

	//vk::CommandBuffer cmd = frame->cmd;

	//cmd.beginDebugUtilsLabelEXT({ "CubemapDX12-Offscreen::cmd" });

	//vk::Viewport viewport{};
	//viewport.x = 0.0f;
	//viewport.y = 0.0f;
	//viewport.width = static_cast<float>(width);
	//viewport.height = static_cast<float>(height);
	//viewport.minDepth = 0.0f;
	//viewport.maxDepth = 1.0f;

	//vk::Rect2D scissor{};
	//scissor.offset = vk::Offset2D{ 0, 0 };
	//scissor.extent = vk::Extent2D{ width, height };

	//cmd.setViewport(0, 1, &viewport);
	//cmd.setScissor(0, 1, &scissor);

	//cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineOffscreen_.getPipeline());

	//vk::Buffer vertBuffer = vertexBuffer_.getBuffer();
	//vk::DeviceSize offset = 0;
	//cmd.bindVertexBuffers(0, 1, &vertBuffer, &offset);

	//glm::mat4 viewStrippedTranslation = glm::mat4(glm::mat3(view));

	//if (time > 0.0f)
	//{
	//	float speed = 0.005f;

	//	glm::mat4 skyRot = glm::rotate(glm::mat4(1.0f),
	//		time * speed,
	//		glm::vec3(0.0f, 1.0f, 0.0f));
	//	viewStrippedTranslation = viewStrippedTranslation * glm::mat4(glm::mat3(skyRot));
	//}

	//CubemapUBO ubo{};
	//ubo.u_dayNightMix = glm::clamp((sunDir.y + 0.15f) / 0.30f, 0.0f, 1.0f);
	//ubo.u_view = viewStrippedTranslation;
	//ubo.u_proj = projection;

	//uboBuffersOffscreen_[frame->frameIndex].upload(&ubo, sizeof(CubemapUBO));

	//vk::DescriptorSet descSet = descriptorSetsOffscreen_[frame->frameIndex].getSet();
	//cmd.bindDescriptorSets(
	//	vk::PipelineBindPoint::eGraphics,
	//	pipelineOffscreen_.getLayout(),
	//	0,
	//	1, &descSet,
	//	0, nullptr
	//);

	//cmd.draw(vertexCount_, 1, 0, 0);

	//cmd.endDebugUtilsLabelEXT();
} // end of renderOffscreen()


//--- PRIVATE ---//
void CubemapDX12::createVertexBuffer()
{
	vertexCount_ = static_cast<uint32_t>(SKYBOX_VERTICES.size() / 3);

	const uint64_t bufferSize = sizeof(float) * SKYBOX_VERTICES.size();

	vertexBuffer_.create(
		bufferSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE
	);

	vertexBuffer_.upload(
		SKYBOX_VERTICES.data(), 
		bufferSize
	);
} // end of createVertexBuffer()

void CubemapDX12::createUBOs()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_[i].create(
			sizeof(CubemapUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);

		uboBuffersOffscreen_[i].create(
			sizeof(CubemapUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);
	} // end for
} // end of createUBOs()

void CubemapDX12::createDescriptorSets()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		// normal descriptor set
		{
			DescriptorBindingDX12 uboBinding{
				.binding = TO_API_FORM(CubemapBinding::UBO),
				.type = DescriptorTypeDX12::UniformBuffer,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_ALL
			};

			DescriptorBindingDX12 nightTexBinding{
				.binding = TO_API_FORM(CubemapBinding::NightSkyboxTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};

			DescriptorBindingDX12 dayTexBinding{
				.binding = TO_API_FORM(CubemapBinding::DaySkyboxTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};

			descriptorSets_[i].createLayout({
				uboBinding,
				nightTexBinding,
				dayTexBinding
				});

			descriptorSets_[i].createPool(3);
			descriptorSets_[i].allocate();

			descriptorSets_[i].setDebugName(
				L"CubemapDX12-Default::DescriptorSet frame " + std::to_wstring(i)
			);

			descriptorSets_[i].writeUniformBuffer(
				TO_API_FORM(CubemapBinding::UBO),
				uboBuffers_[i],
				sizeof(CubemapUBO)
			);

			descriptorSets_[i].writeTextureSRV(
				TO_API_FORM(CubemapBinding::NightSkyboxTex),
				cubemapTextureNight_.image(),
				D3D12_SRV_DIMENSION_TEXTURECUBE
			);

			descriptorSets_[i].writeTextureSRV(
				TO_API_FORM(CubemapBinding::DaySkyboxTex),
				cubemapTextureDay_.image(),
				D3D12_SRV_DIMENSION_TEXTURECUBE
			);
		}

		// offscreen descriptor set
		{
			DescriptorBindingDX12 uboBinding{
				.binding = TO_API_FORM(CubemapBinding::UBO),
				.type = DescriptorTypeDX12::UniformBuffer,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_ALL
			};

			DescriptorBindingDX12 nightTexBinding{
				.binding = TO_API_FORM(CubemapBinding::NightSkyboxTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};

			DescriptorBindingDX12 dayTexBinding{
				.binding = TO_API_FORM(CubemapBinding::DaySkyboxTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};

			descriptorSetsOffscreen_[i].createLayout({
				uboBinding,
				nightTexBinding,
				dayTexBinding
				});

			descriptorSetsOffscreen_[i].createPool(3);
			descriptorSetsOffscreen_[i].allocate();

			descriptorSetsOffscreen_[i].setDebugName(
				L"CubemapDX12-Offscreen::DescriptorSet frame " + std::to_wstring(i)
			);

			descriptorSetsOffscreen_[i].writeUniformBuffer(
				TO_API_FORM(CubemapBinding::UBO),
				uboBuffersOffscreen_[i],
				sizeof(CubemapUBO)
			);

			descriptorSetsOffscreen_[i].writeTextureSRV(
				TO_API_FORM(CubemapBinding::NightSkyboxTex),
				cubemapTextureNight_.image(),
				D3D12_SRV_DIMENSION_TEXTURECUBE
			);

			descriptorSetsOffscreen_[i].writeTextureSRV(
				TO_API_FORM(CubemapBinding::DaySkyboxTex),
				cubemapTextureDay_.image(),
				D3D12_SRV_DIMENSION_TEXTURECUBE
			);
		}
	} // end for
} // end of createDescriptorSets()

void CubemapDX12::createPipeline()
{
	// normal pipeline
	GraphicsPipelineDescDX12 desc{
		.vertShader = shader_->vertShader(),
		.fragShader = shader_->fragShader(),

		.rootSignature = descriptorSets_[0].getRootSignature(),

		.inputElements =
		{
			D3D12_INPUT_ELEMENT_DESC{
				.SemanticName = "POSITION",
				.SemanticIndex = 0,
				.Format = DXGI_FORMAT_R32G32B32_FLOAT,
				.InputSlot = 0,
				.AlignedByteOffset = 0,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0
			}
		},


		.cullMode = D3D12_CULL_MODE_FRONT,
		.frontCCW = FALSE,

		.depthTestEnable = TRUE,
		.depthWriteEnable = FALSE,
		.depthCompareFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,

		//.colorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
		//.depthFormat = DXGI_FORMAT_D32_FLOAT,
		.colorFormat = dx_->getSwapChainImageFormat(),
		.depthFormat = dx_->getDepthFormat(),
	};
	
	pipeline_.create(desc);
	pipeline_.setDebugName(L"CubemapDX12-Default::Pipeline");


	// offscreen pipeline
	pipelineOffscreen_.create(desc);
	pipelineOffscreen_.setDebugName(L"CubemapDX12 - Offscreen::Pipeline");
} // end of createPipeline()