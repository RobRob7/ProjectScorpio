#include "ray_tracing_shader_vk.h"

#include <filesystem>
#include <stdexcept>
#include <fstream>
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
RayTracingShaderModuleVk::RayTracingShaderModuleVk(
	vk::Device device,
	std::string_view rayGenPathFile,
	std::string_view missPathFile,
	std::vector<HitGroupFilePath> hitGroupPathFiles
)
	: device_(device)
{
	if (!device_)
	{
		throw std::runtime_error("RayTracingShaderModuleVk: device is null!");
	}

	const auto rayGenFullPath = std::filesystem::path(RESOURCES_PATH) / "shader" / rayGenPathFile;
	const auto missFullPath = std::filesystem::path(RESOURCES_PATH) / "shader" / missPathFile;

	rayGenShaderModule_ = createShaderModule(ReadFile(rayGenFullPath));
	missShaderModule_ = createShaderModule(ReadFile(missFullPath));

	hitGroupShaderModules_.reserve(hitGroupPathFiles.size());
	for (uint32_t i = 0; i < hitGroupPathFiles.size(); ++i)
	{
		if (hitGroupPathFiles[i].closestHitPath.empty() &&
			hitGroupPathFiles[i].anyHitPath.empty())
		{
			throw std::runtime_error(
				"RayTracingShaderModuleVk: hit group has no closest-hit or any-hit shader path!"
			);
		}

		HitGroupShaderModules modules{};

		if (!hitGroupPathFiles[i].closestHitPath.empty())
		{
			const auto closestHitFullPath =
				std::filesystem::path(RESOURCES_PATH) / "shader" / hitGroupPathFiles[i].closestHitPath;

			modules.closestHitShaderModule =
				createShaderModule(ReadFile(closestHitFullPath));
		}

		if (!hitGroupPathFiles[i].anyHitPath.empty())
		{
			const auto anyHitFullPath =
				std::filesystem::path(RESOURCES_PATH) / "shader" / hitGroupPathFiles[i].anyHitPath;

			modules.anyHitShaderModule =
				createShaderModule(ReadFile(anyHitFullPath));
		}

		hitGroupShaderModules_.push_back(std::move(modules));
	} // end for
} // end of constructor

RayTracingShaderModuleVk::~RayTracingShaderModuleVk() noexcept = default;


//--- PRIVATE ---//
vk::UniqueShaderModule RayTracingShaderModuleVk::createShaderModule(const std::vector<uint32_t>& code)
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
		throw std::runtime_error("RaytracingShaderModuleVkcreateShaderModuleUnique failed: " + vk::to_string(rv.result));
	}

	return std::move(rv.value);
} // end of createShaderModule()