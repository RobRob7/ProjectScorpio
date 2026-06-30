#include "compute_pipeline_dx12.h"

#include "dx12_main.h"
#include "utils_dx12.h"

#include <stdexcept>
#include <string>

//--- PUBLIC ---//
ComputePipelineDX12::ComputePipelineDX12(DX12Main& dx)
	: dx_(&dx)
{
} // end of constructor

ComputePipelineDX12::~ComputePipelineDX12() = default;

void ComputePipelineDX12::setDebugName(const std::wstring& name)
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

void ComputePipelineDX12::create(const ComputePipelineDescDX12& desc)
{
	destroy();

	if (!desc.computeShader.pShaderBytecode || desc.computeShader.BytecodeLength == 0)
	{
		throw std::runtime_error(
			"ComputePipelineDX12::create - missing compute shader bytecode"
		);
	}

	if (desc.rootSignature)
	{
		rootSignature_ = desc.rootSignature;
	}
	else
	{
		D3D12_ROOT_SIGNATURE_DESC rootDesc{};
		rootDesc.NumParameters = 0;
		rootDesc.pParameters = nullptr;
		rootDesc.NumStaticSamplers = 0;
		rootDesc.pStaticSamplers = nullptr;
		rootDesc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

		HRESULT hr = D3D12SerializeRootSignature(
			&rootDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob
		);

		if (FAILED(hr))
		{
			if (errorBlob)
			{
				std::string error(
					static_cast<const char*>(errorBlob->GetBufferPointer()),
					errorBlob->GetBufferSize()
				);
				throw std::runtime_error(
					"ComputePipelineDX12::create - failed to serialize empty root signature: " + error
				);
			}

			DX12Utils::ThrowIfFailed(
				dx_->getDevice(),
				hr,
				"ComputePipelineDX12::create - failed to serialize empty root signature"
			);
		}

		DX12Utils::ThrowIfFailed(
			dx_->getDevice(),
			dx_->getDevice()->CreateRootSignature(
				0,
				signatureBlob->GetBufferPointer(),
				signatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&rootSignature_)
			),
			"ComputePipelineDX12::create - failed to create empty root signature"
		);
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};

	// root sig
	psoDesc.pRootSignature = rootSignature_;

	// shaders
	psoDesc.CS = desc.computeShader;

	// misc
	psoDesc.NodeMask = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	HRESULT hr = dx_->getDevice()->CreateComputePipelineState(
		&psoDesc,
		IID_PPV_ARGS(&pipelineState_)
	);

#ifdef _DEBUG
	if (FAILED(hr))
	{
		dx_->dumpDebugMessages("CreateComputePipelineState failed");
	}
#endif

	DX12Utils::ThrowIfFailed(
		dx_->getDevice(),
		hr,
		"ComputePipelineDX12::create - failed to create Compute pipeline state"
	);
} // end of create()


//--- PRIVATE ---//
void ComputePipelineDX12::destroy()
{
	pipelineState_.Reset();
	rootSignature_ = nullptr;
} // end of destroy()
