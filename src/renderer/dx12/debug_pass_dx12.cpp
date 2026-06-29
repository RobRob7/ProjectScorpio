#include "debug_pass_dx12.h"

#include "dx12_main.h"

#include "frame_context_dx12.h"
#include "utils_dx12.h"

#include "bindings.h"

#include "image_dx12.h"
#include "shader_dx12.h"
#include "buffer_dx12.h"
#include "descriptor_set_dx12.h"
#include "graphics_pipeline_dx12.h"

#include <memory>
#include <cstdint>

//--- PUBLIC ---//
DebugPassDX12::DebugPassDX12(DX12Main& dx)
	: dx_(&dx),
	pipeline_(dx)
{
	const uint32_t frames = dx.getMaxFramesInFlight();

	uboBuffers_.reserve(frames);
	descriptorSets_.reserve(frames);

	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_.emplace_back(dx);
		descriptorSets_.emplace_back(dx);
	} // end for
} // end of constructor

DebugPassDX12::~DebugPassDX12() = default;

void DebugPassDX12::init()
{
	shader_ = std::make_unique<ShaderDX12>(
		"hlsl/debugpass/debugpass.vert.cso",
		"hlsl/debugpass/debugpass.frag.cso"
	);

	createResources();
	createDescriptorSets();
	createPipeline();
} // end of init()

void DebugPassDX12::resize()
{
	//refreshInputs();
} // end of resize()

void DebugPassDX12::render(
	const Debug_Constants::DebugPassUBO& uboData,
	FrameContextDX12& frame
)
{
	updateDescriptorSet(frame.frameIndex);

	ID3D12GraphicsCommandList* cmd = frame.cmd;
	cmd->SetName({ L"DebugPassDX12:cmd" });

	normalImage_->transitionToShaderRead(cmd);
	depthImage_->transitionToShaderRead(cmd);

	D3D12_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(frame.width);
	viewport.Height = static_cast<float>(frame.height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissor{};
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = static_cast<LONG>(frame.width);
	scissor.bottom = static_cast<LONG>(frame.height);

	cmd->RSSetViewports(1, &viewport);
	cmd->RSSetScissorRects(1, &scissor);

	D3D12_CPU_DESCRIPTOR_HANDLE colorRTV = frame.colorRTV;

	cmd->OMSetRenderTargets(
		1,
		&colorRTV,
		FALSE,
		nullptr
	);

	const float clearColor[4] =
	{
		0.0f, 0.0f, 0.0f, 1.0f
	};
	cmd->ClearRenderTargetView(
		colorRTV,
		clearColor,
		0,
		nullptr
	);

	uboBuffers_[frame.frameIndex].upload(&uboData, sizeof(uboData));

	DescriptorSetDX12& set = descriptorSets_[frame.frameIndex];
	ID3D12DescriptorHeap* heaps[] =
	{
		set.getDescriptorHeap()
	};

	cmd->SetDescriptorHeaps(1, heaps);
	cmd->SetGraphicsRootSignature(pipeline_.getRootSignature());
	cmd->SetPipelineState(pipeline_.getPipeline());

	cmd->SetGraphicsRootDescriptorTable(
		set.getDescriptorTableRootIndex(),
		set.getTableGPUHandle()
	);

	cmd->DrawInstanced(
		3, 
		1, 
		0, 
		0
	);
} // end of render()


//--- PRIVATE ---//
void DebugPassDX12::updateDescriptorSet(uint32_t frameIndex)
{
	DescriptorSetDX12& set = descriptorSets_[frameIndex];
	if (!set.valid())
	{
		return;
	}

	if (normalImage_ && normalImage_->valid())
	{
		set.writeTextureSRV(
			TO_API_FORM(DebugBinding::GNormalTex),
			*normalImage_
		);
	}

	if (depthImage_ && depthImage_->valid())
	{
		set.writeTextureSRV(
			TO_API_FORM(DebugBinding::GDepthTex),
			*depthImage_
		);
	}

	if (shadowMapImage_ && shadowMapImage_->valid())
	{
		set.writeTextureSRV(
			TO_API_FORM(DebugBinding::ShadowMapTex),
			*shadowMapImage_
		);
	}

	if (rtDepthImage_ && rtDepthImage_->valid())
	{
		set.writeTextureSRV(
			TO_API_FORM(DebugBinding::RTDepthTex),
			*rtDepthImage_
		);
	}
} // end of updateDescriptorSet()

void DebugPassDX12::createResources()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_[i].create(
			sizeof(Debug_Constants::DebugPassUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);
	} // end for
} // end of createResources()

void DebugPassDX12::createDescriptorSets()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		DescriptorBindingDX12 uboBinding{
			.binding = TO_API_FORM(DebugBinding::UBO),
			.type = DescriptorTypeDX12::UniformBuffer,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_PIXEL
		};
		DescriptorBindingDX12 normalBinding{
			.binding = TO_API_FORM(DebugBinding::GNormalTex),
			.type = DescriptorTypeDX12::TextureSRV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_PIXEL
		};
		DescriptorBindingDX12 depthBinding{
			.binding = TO_API_FORM(DebugBinding::GDepthTex),
			.type = DescriptorTypeDX12::TextureSRV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_PIXEL
		};
		DescriptorBindingDX12 shadowMapBinding{
			.binding = TO_API_FORM(DebugBinding::ShadowMapTex),
			.type = DescriptorTypeDX12::TextureSRV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_PIXEL
		};
		DescriptorBindingDX12 rtDepthBinding{
			.binding = TO_API_FORM(DebugBinding::RTDepthTex),
			.type = DescriptorTypeDX12::TextureSRV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_PIXEL
		};

		descriptorSets_[i].createLayout({
			uboBinding,
			normalBinding,
			depthBinding,
			shadowMapBinding,
			rtDepthBinding
			});

		descriptorSets_[i].createPool(5);
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			L"DebugPassDX12::descriptorSets_ frame " + std::to_wstring(i)
		);

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(DebugBinding::UBO),
			uboBuffers_[i],
			sizeof(Debug_Constants::DebugPassUBO)
		);
	} 
} // end of createDescriptorSets()

void DebugPassDX12::createPipeline()
{
	GraphicsPipelineDescDX12 desc{
	.vertShader = shader_->vertShader(),
	.fragShader = shader_->fragShader(),

	.rootSignature = descriptorSets_[0].getRootSignature(),

	.cullMode = D3D12_CULL_MODE_NONE,
	.frontCCW = FALSE,

	.depthTestEnable = FALSE,
	.depthWriteEnable = FALSE,

	.colorFormat = dx_->getSwapChainImageFormat()
	};

	pipeline_.create(desc);
	pipeline_.setDebugName(L"DebugPassDX12::Pipeline");
} // end of createPipeline()