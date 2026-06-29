#include "gbuffer_pass_dx12.h"

#include "frame_context_dx12.h"

#include "render_inputs.h"
#include "chunk_manager.h"

#include "constants.h"
#include "bindings.h"
#include "shader_dx12.h"
#include "dx12_main.h"
#include "image_dx12.h"

#include "chunk_draw_list.h"
#include "i_chunk_mesh_gpu.h"

#include <cstdint>

//[using namespace Gbuffer_Constants;
//using namespace World; 

//--- PUBLIC ---//
GBufferPassDX12::GBufferPassDX12(DX12Main& dx)
	: dx_(&dx),
	gNormalImage_(dx),
	gDepthImage_(dx),
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

GBufferPassDX12::~GBufferPassDX12() = default;

void GBufferPassDX12::init()
{
	shader_ = std::make_unique<ShaderDX12>(
		"hlsl/gbuffer/gbuffer.vert.cso",
		"hlsl/gbuffer/gbuffer.frag.cso"
	);

	createAttachments();
	createResources();
	createDescriptorSet();
	createPipeline();
} // end of init()

void GBufferPassDX12::resize()
{
	createAttachments();
} // end of resize()

void GBufferPassDX12::render(
	const Gbuffer_Constants::GbufferUBO uboData,
	const RenderInputs& in,
	const FrameContextDX12& frame
)
{
	ID3D12GraphicsCommandList* cmd = frame.cmd;
	cmd->SetName({ L"GBufferPassDX12::cmd" });

	gNormalImage_.transitionToRenderTarget(cmd);
	gDepthImage_.transitionToDepthWrite(cmd);

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

	D3D12_CPU_DESCRIPTOR_HANDLE colorRTV = gNormalImage_.rtvCPU();
	D3D12_CPU_DESCRIPTOR_HANDLE depthDSV = gDepthImage_.dsvCPU();

	cmd->OMSetRenderTargets(
		1,
		&colorRTV,
		FALSE,
		&depthDSV
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

	cmd->ClearDepthStencilView(
		depthDSV,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
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

	// render chunks
	const ChunkDrawList& list = in.world->getOpaqueDrawList();
	for (const auto& item : list.items)
	{
		Chunk_Constants::ChunkPushConstants pc{};
		pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

		set.setGraphicsPushConstants(
			cmd,
			0,
			pc
		);

		item.gpu->drawOpaque(nullptr, &frame);
	} // end for
	
	gNormalImage_.transitionToShaderRead(cmd);
	gDepthImage_.transitionToShaderRead(cmd);
} // end of render()


//--- PRIVATE ---//
void GBufferPassDX12::createAttachments()
{
	Extent2D extent = dx_->getSwapChainExtent();

	// NORMAL
	D3D12_CLEAR_VALUE colorClear{
		.Format = normalFormat_,
		.Color = {0.0f, 0.0f, 0.0f, 1.0f},
	};
	gNormalImage_.createImage(
		extent.width,
		extent.height,
		1,
		false,
		normalFormat_,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&colorClear
	);
	gNormalImage_.createRTV();
	gNormalImage_.setDebugName(L"GBufferPassDX12::NormalImage");


	// DEPTH
	gDepthImage_.createSampledDepthImage(
		extent.width,
		extent.height
	);
	gDepthImage_.setDebugName(L"GBufferPassDX12::DepthImage");
} // end of createAttachments()

void GBufferPassDX12::createResources()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_[i].create(
			sizeof(Gbuffer_Constants::GbufferUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);
	} // end for
} // end of createResources()

void GBufferPassDX12::createDescriptorSet()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		DescriptorBindingDX12 uboBinding{
			.binding = TO_API_FORM(GbufferBinding::UBO),
			.type = DescriptorTypeDX12::UniformBuffer,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_VERTEX
		};
		PushConstantRangeDX12 chunkPushConstants{
				.binding = 0,
				.num32BitValues = sizeof(Chunk_Constants::ChunkPushConstants) / 4,
				.registerSpace = 1,
				.visibility = D3D12_SHADER_VISIBILITY_VERTEX
		};

		descriptorSets_[i].createLayout(
			{
				uboBinding
			},
			{
				chunkPushConstants
			}
		);

		descriptorSets_[i].createPool(1);
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			L"GBufferPassDX12-GBuffer::DescriptorSet frame " + std::to_wstring(i)
		);

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(GbufferBinding::UBO),
			uboBuffers_[i],
			sizeof(Gbuffer_Constants::GbufferUBO)
		);
	} // end for
} // end of createDescriptorSet()

void GBufferPassDX12::createPipeline()
{
	GraphicsPipelineDescDX12 desc{
		.vertShader = shader_->vertShader(),
		.fragShader = shader_->fragShader(),

		.rootSignature = descriptorSets_[0].getRootSignature(),

		.inputElements =
		{
			D3D12_INPUT_ELEMENT_DESC{
				.SemanticName = "POSITION",
				.SemanticIndex = 0,
				.Format = DXGI_FORMAT_R32_UINT,
				.InputSlot = 0,
				.AlignedByteOffset = 0,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0
			}
		},

		.cullMode = D3D12_CULL_MODE_BACK,
		.frontCCW = FALSE,

		.depthTestEnable = TRUE,
		.depthWriteEnable = TRUE,
		.depthCompareFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,

		.colorFormat = normalFormat_,
		.depthFormat = depthFormat_,
	};

	pipeline_.create(desc);
	pipeline_.setDebugName(L"GBufferPassDX12-GBuffer::Pipeline");
} // end of createPipeline()
