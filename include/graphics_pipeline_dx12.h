#ifndef GRAPHICS_PIPELINE_DX12_H
#define GRAPHICS_PIPELINE_DX12_H

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

struct GraphicsPipelineDescDX12
{
	// shaders
	D3D12_SHADER_BYTECODE vertShader{};
	D3D12_SHADER_BYTECODE fragShader{};

	// root signature
	ID3D12RootSignature* rootSignature{ nullptr };

	// vertex input
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

	// primitive assembly
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };

	// rasterizer
	D3D12_FILL_MODE fillMode{ D3D12_FILL_MODE_SOLID };
	D3D12_CULL_MODE cullMode{ D3D12_CULL_MODE_BACK };
	BOOL frontCCW{ TRUE };
	BOOL depthClipEnable{ TRUE };

	// multisampling
	UINT sampleCount{ 1 };
	UINT sampleQuality{ 0 };

	// blending
	BOOL blendEnable{ FALSE };
	UINT8 colorWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL };

	// depth
	BOOL depthTestEnable{ TRUE };
	BOOL depthWriteEnable{ TRUE };
	D3D12_COMPARISON_FUNC depthCompareFunc{
		D3D12_COMPARISON_FUNC_LESS_EQUAL
	};

	// render target formats
	DXGI_FORMAT colorFormat{ DXGI_FORMAT_UNKNOWN };
	DXGI_FORMAT depthFormat{ DXGI_FORMAT_UNKNOWN };
};

class GraphicsPipelineDX12
{
public:
	explicit GraphicsPipelineDX12(DX12Main& dx);
	~GraphicsPipelineDX12();

	GraphicsPipelineDX12(const GraphicsPipelineDX12&) = delete;
	GraphicsPipelineDX12& operator=(const GraphicsPipelineDX12&) = delete;

	GraphicsPipelineDX12(GraphicsPipelineDX12&&) noexcept = default;
	GraphicsPipelineDX12& operator=(GraphicsPipelineDX12&&) noexcept = default;

	void setDebugName(const std::wstring& name);

	void create(const GraphicsPipelineDescDX12& desc);

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
