#include "compute_shader_dx12.h"

#include <filesystem>
#include <stdexcept>
#include <fstream>

//--- HELPER ---//
static std::vector<uint8_t> ReadFile(const std::filesystem::path& pathFile)
{
	std::ifstream file(pathFile, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("ComputeShaderDX12::readFile - failed to open file: " + pathFile.string());
	}

	const std::streamsize fileSize = file.tellg();
	if (fileSize <= 0)
	{
		throw std::runtime_error("ComputeShaderDX12::readFile - shader file is empty (or tellg failed)");
	}

	std::vector<std::uint8_t> buffer(static_cast<size_t>(fileSize));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

	if (!file)
	{
		throw std::runtime_error(
			"ComputeShaderDX12::readFile - failed to read file: " + pathFile.string()
		);
	}

	return buffer;
} // end of readFile()


//--- PUBLIC ---//
ComputeShaderDX12::ComputeShaderDX12(std::string_view computePathFile)
{
	const auto computeFullPath = std::filesystem::path(RESOURCES_PATH) / "shader" / computePathFile;

	computeCode_ = ReadFile(computeFullPath);
} // end of constructor

ComputeShaderDX12::~ComputeShaderDX12() noexcept = default;
