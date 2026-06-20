#include "graphics_pipeline_dx12.h"

#include "dx12_main.h"
#include "utils_dx12.h"

#include <stdexcept>
#include <string>

//--- PUBLIC ---//
GraphicsPipelineDX12::GraphicsPipelineDX12(DX12Main& dx)
	: dx_(&dx)
{
} // end of constructor

GraphicsPipelineDX12::~GraphicsPipelineDX12() = default;

void GraphicsPipelineDX12::setDebugName(const std::wstring& name)
{
	if (pipelineState_)
	{
		dx_->setDebugName(pipelineState_.Get(), name.c_str());
	}

	if (rootSignature_)
	{
		std::wstring rsName = name + L" Root Signature";
		dx_->setDebugName(rootSignature_, rsName.c_str());
	}
} // end of setDebugName()

void GraphicsPipelineDX12::create(const GraphicsPipelineDescDX12& desc)
{
	destroy();

	if (!desc.vertShader.pShaderBytecode || desc.vertShader.BytecodeLength == 0)
	{
		throw std::runtime_error(
			"GraphicsPipelineDX12::create - missing vertex shader bytecode"
		);
	}

	if (!desc.fragShader.pShaderBytecode || desc.fragShader.BytecodeLength == 0)
	{
		throw std::runtime_error(
			"GraphicsPipelineDX12::create - missing fragment shader bytecode"
		);
	}

	if (!desc.rootSignature)
	{
		throw std::runtime_error(
			"GraphicsPipelineDX12::create - root signature is null"
		);
	}

	if (desc.colorFormat == DXGI_FORMAT_UNKNOWN)
	{
		throw std::runtime_error(
			"GraphicsPipelineDX12::create - colorFormat is DXGI_FORMAT_UNKNOWN"
		);
	}

	if (desc.depthTestEnable && desc.depthFormat == DXGI_FORMAT_UNKNOWN)
	{
		throw std::runtime_error(
			"GraphicsPipelineDX12::create - depthFormat is DXGI_FORMAT_UNKNOWN while depth is enabled"
		);
	}

	rootSignature_ = desc.rootSignature;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

	// root sig
	psoDesc.pRootSignature = rootSignature_;

	// shaders
	psoDesc.VS = desc.vertShader;
	psoDesc.PS = desc.fragShader;

	// vertex input
	psoDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = desc.inputElements.empty()
			? nullptr
			: desc.inputElements.data(),
		.NumElements = static_cast<UINT>(desc.inputElements.size())
	};

	// rasterizer
	D3D12_RASTERIZER_DESC rasterizerDesc{
		.FillMode = desc.fillMode,
		.CullMode = desc.cullMode,
		.FrontCounterClockwise = desc.frontCCW,
		.DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
		.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		.DepthClipEnable = desc.depthClipEnable,
		.MultisampleEnable = FALSE,
		.AntialiasedLineEnable = FALSE,
		.ForcedSampleCount = 0,
		.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	};
	psoDesc.RasterizerState = rasterizerDesc;

	// blending
	D3D12_RENDER_TARGET_BLEND_DESC rtBlend{};
	rtBlend.BlendEnable = desc.blendEnable;
	rtBlend.LogicOpEnable = FALSE;
	rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
	rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
	rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
	rtBlend.RenderTargetWriteMask = desc.colorWriteMask;

	D3D12_BLEND_DESC blend{};
	blend.AlphaToCoverageEnable = FALSE;
	blend.IndependentBlendEnable = FALSE;
	blend.RenderTarget[0] = rtBlend;

	psoDesc.BlendState = blend;

	// depth/stencil
	D3D12_DEPTH_STENCIL_DESC depth{};
	depth.DepthEnable = desc.depthTestEnable;
	depth.DepthWriteMask = desc.depthWriteEnable
		? D3D12_DEPTH_WRITE_MASK_ALL
		: D3D12_DEPTH_WRITE_MASK_ZERO;
	depth.DepthFunc = desc.depthCompareFunc;
	depth.StencilEnable = FALSE;
	depth.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depth.FrontFace =
	{
		.StencilFailOp = D3D12_STENCIL_OP_KEEP,
		.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
		.StencilPassOp = D3D12_STENCIL_OP_KEEP,
		.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
	};
	depth.BackFace = depth.FrontFace;

	psoDesc.DepthStencilState = depth;

	// formats
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = desc.colorFormat;
	psoDesc.DSVFormat = desc.depthTestEnable
		? desc.depthFormat
		: DXGI_FORMAT_UNKNOWN;

	// misc
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = desc.primitiveTopologyType;
	psoDesc.SampleDesc =
	{
		.Count = desc.sampleCount,
		.Quality = desc.sampleQuality
	};
	psoDesc.NodeMask = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	DX12Utils::ThrowIfFailed(
		dx_->getDevice()->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS(&pipelineState_)
		),
		"GraphicsPipelineDX12::create - failed to create graphics pipeline state"
	);
} // end of create()


//--- PRIVATE ---//
void GraphicsPipelineDX12::destroy()
{
	pipelineState_.Reset();
	rootSignature_ = nullptr;
} // end of destroy()
