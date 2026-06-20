#ifndef SHADER_DX12_H
#define SHADER_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d12.h>

#include <string_view>
#include <vector>
#include <cstdint>

class ShaderDX12
{
public:
	ShaderDX12(
		std::string_view vertexPathFile,
		std::string_view fragPathFile
	);
	~ShaderDX12();

	ShaderDX12(const ShaderDX12&) = delete;
	ShaderDX12& operator=(const ShaderDX12&) = delete;

	ShaderDX12(ShaderDX12&& other) noexcept = default;
	ShaderDX12& operator=(ShaderDX12&& other) noexcept = default;

	D3D12_SHADER_BYTECODE vertShader() const noexcept 
	{ 
		return D3D12_SHADER_BYTECODE{
			.pShaderBytecode = vertexCode_.data(),
			.BytecodeLength = vertexCode_.size()
		};
	} // end of vertShader()

	D3D12_SHADER_BYTECODE fragShader() const noexcept 
	{
		return D3D12_SHADER_BYTECODE{
			.pShaderBytecode = fragCode_.data(),
			.BytecodeLength = fragCode_.size()
		};
	} // end of fragShader()

private:
	std::vector<std::uint8_t> vertexCode_;
	std::vector<std::uint8_t> fragCode_;
};

#endif
