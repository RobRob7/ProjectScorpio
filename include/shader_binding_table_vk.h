#ifndef SHADER_BINDING_TABLE_VK_H
#define SHADER_BINDING_TABLE_VK_H

#include <vulkan/vulkan.hpp>

#include "buffer_vk.h"

#include <cstdint>

class VulkanMain;

class ShaderBindingTableVk
{
public:
	explicit ShaderBindingTableVk(VulkanMain& vk);
	~ShaderBindingTableVk();

	void create(
		vk::Pipeline rtPipeline,
		uint32_t groupCount,
		uint32_t rayGenGroupIndex,
		uint32_t missGroupIndex,
		uint32_t hitGroupIndex
	);

	const vk::StridedDeviceAddressRegionKHR& rayGenRegion() const { return rayGenRegion_; }
	const vk::StridedDeviceAddressRegionKHR& missRegion() const { return missRegion_; }
	const vk::StridedDeviceAddressRegionKHR& hitRegion() const { return hitRegion_; }
	const vk::StridedDeviceAddressRegionKHR& callableRegion() const { return callableRegion_; }

private:
	VulkanMain& vk_;
	
	BufferVk buffer_;

	vk::StridedDeviceAddressRegionKHR rayGenRegion_{};
	vk::StridedDeviceAddressRegionKHR missRegion_{};
	vk::StridedDeviceAddressRegionKHR hitRegion_{};
	vk::StridedDeviceAddressRegionKHR callableRegion_{};
};

#endif
