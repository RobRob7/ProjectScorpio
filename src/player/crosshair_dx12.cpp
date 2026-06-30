#include "crosshair_dx12.h"

#include "constants.h"
#include "dx12_main.h"
#include "frame_context_dx12.h"

#include "image_dx12.h"
#include "buffer_dx12.h"
#include "graphics_pipeline_dx12.h"
#include "shader_dx12.h"

#include <memory>

using namespace Crosshair_Constants;

//--- PUBLIC ---//
CrosshairDX12::CrosshairDX12(DX12Main& dx)
	: dx_(&dx),
	vBuffer_(dx),
	pipeline_(dx)
{
} // end of constructor

CrosshairDX12::~CrosshairDX12() = default;

void CrosshairDX12::init()
{

	shader_ = std::make_unique<ShaderDX12>(
		"hlsl/crosshair/crosshair.vert.cso", 
		"hlsl/crosshair/crosshair.frag.cso"
	);

	createResources();
	createPipeline();
} // end of init()

void CrosshairDX12::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12
)
{
	if (!frameDX12->cmd || !vBuffer_.valid())
		return;

	if (!vBuffer_.valid() ||
		!pipeline_.valid()) return;

	const FrameContextDX12& frame = *frameDX12;

	ID3D12GraphicsCommandList* cmd = frame.cmd;
	cmd->SetName(L"CrosshairDX12::cmd");

	dx_->beginGPUEvent(cmd, L"CrosshairDX12::render");
	{
		D3D12_VIEWPORT viewport{
			.TopLeftX = 0.0f,
			.TopLeftY = 0.0f,
			.Width = static_cast<float>(frame.width),
			.Height = static_cast<float>(frame.height),
			.MinDepth = 0.0f,
			.MaxDepth = 1.0f
		};
		D3D12_RECT scissor{
			.left = 0,
			.top = 0,
			.right = static_cast<LONG>(frame.width),
			.bottom = static_cast<LONG>(frame.height)
		};
		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissor);

		D3D12_CPU_DESCRIPTOR_HANDLE colorRTV = frame.colorRTV;
		cmd->OMSetRenderTargets(
			1,
			&colorRTV,
			FALSE,
			nullptr
		);

		cmd->SetGraphicsRootSignature(pipeline_.getRootSignature());
		cmd->SetPipelineState(pipeline_.getPipeline());

		D3D12_VERTEX_BUFFER_VIEW vbView{
			.BufferLocation = vBuffer_.getGPUVirtualAddress(),
			.SizeInBytes = static_cast<UINT>(vBuffer_.size()),
			.StrideInBytes = sizeof(float) * 2
		};

		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		cmd->IASetVertexBuffers(0, 1, &vbView);
		cmd->DrawInstanced(4, 1, 0, 0);
	}
	dx_->endGPUEvent(cmd);
} // end of render()


//--- PRIVATE ---//
void CrosshairDX12::createResources()
{
	const uint64_t vbSize = sizeof(VERTICES);

	vBuffer_.create(
		vbSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE,
		false
	);

	vBuffer_.upload(VERTICES, vbSize);
	//vBuffer_.setDebugName(L"CrosshairDX12::VertexBuffer");
} // end of createResources()

void CrosshairDX12::createPipeline()
{
	GraphicsPipelineDescDX12 desc{
		.vertShader = shader_->vertShader(),
		.fragShader = shader_->fragShader(),

		.inputElements =
		{
			D3D12_INPUT_ELEMENT_DESC{
				.SemanticName = "POSITION",
				.SemanticIndex = 0,
				.Format = DXGI_FORMAT_R32G32_FLOAT,
				.InputSlot = 0,
				.AlignedByteOffset = 0,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0
			}
		},

		.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
		.cullMode = D3D12_CULL_MODE_NONE,
		.frontCCW = FALSE,

		.depthTestEnable = FALSE,
		.depthWriteEnable = FALSE,

		.colorFormat = dx_->getSwapChainImageFormat()
	};

	pipeline_.create(desc);
	pipeline_.setDebugName(L"CrosshairDX12::Pipeline");
} // end of createPipeline()