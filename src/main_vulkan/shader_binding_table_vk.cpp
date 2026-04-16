#include "shader_binding_table_vk.h"

#include "vulkan_main.h"

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <algorithm>

//--- PUBLIC ---//
ShaderBindingTableVk::ShaderBindingTableVk(VulkanMain& vk)
	: vk_(vk),
	buffer_(vk)
{
} // end of constructor

ShaderBindingTableVk::~ShaderBindingTableVk() = default;

void ShaderBindingTableVk::create(
	vk::Pipeline rtPipeline,
	uint32_t groupCount,
	uint32_t rayGenGroupIndex,
	uint32_t missGroupIndex,
	uint32_t hitGroupIndex
)
{
	if (!rtPipeline)
	{
		throw std::runtime_error("ShaderBindingTableVk::create - rtPipeline is NULL!");
	}

	if (groupCount == 0)
	{
		throw std::runtime_error("ShaderBindingTableVk::create - groupCount must be greater than 0!");
	}

	if (rayGenGroupIndex >= groupCount ||
		missGroupIndex >= groupCount ||
		hitGroupIndex >= groupCount)
	{
		throw std::runtime_error("ShaderBindingTableVk::create - group index out of range!");
	}

	// RT pipeline properties
	vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
	vk::PhysicalDeviceProperties2 props2{};
	props2.pNext = &rtProps;
	vk_.getPhysicalDevice().getProperties2(&props2);

	const uint32_t handleSize = rtProps.shaderGroupHandleSize;
	const uint32_t handleAlignment = rtProps.shaderGroupHandleAlignment;
	const uint32_t baseAlignment = rtProps.shaderGroupBaseAlignment;

	if (handleSize == 0 || handleAlignment == 0 || baseAlignment == 0)
	{
		throw std::runtime_error("ShaderBindingTableVk::create - invalid RT pipeline properties!");
	}

	auto alignUp = [](vk::DeviceSize value, vk::DeviceSize alignment) -> vk::DeviceSize
		{
			return (value + alignment - 1) & ~(alignment - 1);
		};

	const vk::DeviceSize handleSizeAligned = alignUp(handleSize, handleAlignment);

	// get shader group handles from pipeline
	std::vector<uint8_t> handles(static_cast<size_t>(groupCount) * handleSize);

	vk::Result res = vk_.getDevice().getRayTracingShaderGroupHandlesKHR(
		rtPipeline,
		0,
		groupCount,
		handles.size(),
		handles.data()
	);

	if (res != vk::Result::eSuccess)
	{
		throw std::runtime_error("ShaderBindingTableVk::create - getRayTracingShaderGroupHandlesKHR failed: " +
			vk::to_string(res));
	}

	const vk::DeviceSize regionStride = alignUp(handleSizeAligned, baseAlignment);
	const vk::DeviceSize sbtSize = regionStride * 3;

	std::vector<uint8_t> sbtData(static_cast<size_t>(sbtSize), 0);

	auto copyHandleToRecord = [&](uint32_t srcGroupIndex, uint32_t dstRecordIndex)
		{
			const uint8_t* src = handles.data() + static_cast<size_t>(srcGroupIndex) * handleSize;
			uint8_t* dst = sbtData.data() + static_cast<size_t>(dstRecordIndex * regionStride);
			std::memcpy(dst, src, handleSize);
		};

	copyHandleToRecord(rayGenGroupIndex, 0);
	copyHandleToRecord(missGroupIndex, 1);
	copyHandleToRecord(hitGroupIndex, 2);

	// create SBT buffer
	buffer_.create(
		sbtSize,
		vk::BufferUsageFlagBits::eShaderBindingTableKHR |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eHostVisible |
		vk::MemoryPropertyFlagBits::eHostCoherent,
		true
	);
	buffer_.upload(sbtData.data(), sbtSize);

	const vk::DeviceAddress baseAddress = buffer_.getDeviceAddress();

	// build regions for vkCmdTraceRaysKHR
	rayGenRegion_.deviceAddress = baseAddress + regionStride * 0;
	rayGenRegion_.stride = regionStride;
	rayGenRegion_.size = regionStride;

	missRegion_.deviceAddress = baseAddress + regionStride * 1;
	missRegion_.stride = regionStride;
	missRegion_.size = regionStride;

	hitRegion_.deviceAddress = baseAddress + regionStride * 2;
	hitRegion_.stride = regionStride;
	hitRegion_.size = regionStride;

	callableRegion_ = vk::StridedDeviceAddressRegionKHR{};
} // end of create()
