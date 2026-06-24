#ifndef COMPUTE_SHADER_DX12_H
#define COMPUTE_SHADER_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d12.h>

#include <string_view>
#include <vector>
#include <cstdint>

class ComputeShaderDX12
{
public:
	ComputeShaderDX12(std::string_view computePathFile);
	~ComputeShaderDX12();

	ComputeShaderDX12(const ComputeShaderDX12&) = delete;
	ComputeShaderDX12& operator=(const ComputeShaderDX12&) = delete;

	ComputeShaderDX12(ComputeShaderDX12&& other) noexcept = default;
	ComputeShaderDX12& operator=(ComputeShaderDX12&& other) noexcept = default;

	D3D12_SHADER_BYTECODE computeShader() const noexcept 
	{ 
		return D3D12_SHADER_BYTECODE{
			.pShaderBytecode = computeCode_.data(),
			.BytecodeLength = computeCode_.size()
		};
	} // end of computeShader()

private:
	std::vector<std::uint8_t> computeCode_;
};

#endif
