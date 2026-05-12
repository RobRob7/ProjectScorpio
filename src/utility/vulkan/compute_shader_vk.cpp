#include "compute_shader_vk.h"

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <utility>

//--- HELPER ---//
static std::vector<uint32_t> ReadFile(const std::filesystem::path& pathFile)
{
	std::ifstream file(pathFile, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file: " + pathFile.string());
	}

	const std::streamsize fileSize = file.tellg();
	if (fileSize <= 0)
	{
		throw std::runtime_error("shader file is empty (or tellg failed)");
	}

	if ((fileSize % 4) != 0)
	{
		throw std::runtime_error("SPIR-V file size is not a multiple of 4 bytes!");
	}

	std::vector<uint32_t> buffer(static_cast<size_t>(fileSize / 4));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
	if (!file)
	{
		throw std::runtime_error("failed to read file: " + pathFile.string());
	}

	return buffer;
} // end of readFile()


//--- PUBLIC ---//
ComputeShaderModuleVk::ComputeShaderModuleVk(
	vk::Device device,
	std::string_view compShaderPath
	)
	: device_(device)
{
	if (!device_)
	{
		throw std::runtime_error("ComputeShaderModuleVk: device is null!");
	}

	const auto compFullPath = std::filesystem::path(RESOURCES_PATH) / "shader" / compShaderPath;

	compShaderModule_ = createShaderModule(ReadFile(compFullPath));
} // end of constructor

ComputeShaderModuleVk::~ComputeShaderModuleVk() noexcept = default;


//--- PRIVATE ---//
vk::UniqueShaderModule ComputeShaderModuleVk::createShaderModule(const std::vector<uint32_t>& code)
{
	if (code.empty())
	{
		throw std::runtime_error("SPIR-V code is empty");
	}

	vk::ShaderModuleCreateInfo createInfo{};
	createInfo.codeSize = code.size() * sizeof(uint32_t);
	createInfo.pCode = code.data();

	auto rv = device_.createShaderModuleUnique(createInfo);
	if (rv.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("ComputeShaderModuleVk::createShaderModuleUnique failed: " + vk::to_string(rv.result));
	}

	return std::move(rv.value);
} // end of createShaderModule()