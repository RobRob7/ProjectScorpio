#ifndef COMPUTE_PIPELINE_DX12_H
#define COMPUTE_PIPELINE_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>
#include <string>
#include <cstdint>

class DX12Main;

using Microsoft::WRL::ComPtr;

struct ComputePipelineDescDX12
{
	// shaders
	D3D12_SHADER_BYTECODE computeShader{};

	// root signature
	ID3D12RootSignature* rootSignature{ nullptr };
};

class ComputePipelineDX12
{
public:
	explicit ComputePipelineDX12(DX12Main& dx);
	~ComputePipelineDX12();

	ComputePipelineDX12(const ComputePipelineDX12&) = delete;
	ComputePipelineDX12& operator=(const ComputePipelineDX12&) = delete;

	ComputePipelineDX12(ComputePipelineDX12&&) noexcept = default;
	ComputePipelineDX12& operator=(ComputePipelineDX12&&) noexcept = default;

	void setDebugName(const std::wstring& name);

	void create(const ComputePipelineDescDX12& desc);

	bool valid() const { return pipelineState_ != nullptr; }

	ID3D12RootSignature* getRootSignature() const { return rootSignature_; }
	ID3D12PipelineState* getPipeline() const { return pipelineState_.Get(); }

private:
	void destroy();
private:
	DX12Main* dx_{ nullptr };

	ID3D12RootSignature* rootSignature_{ nullptr };
	ComPtr<ID3D12PipelineState> pipelineState_{};
};

#endif
