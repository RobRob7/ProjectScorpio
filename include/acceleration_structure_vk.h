#ifndef ACCELERATION_STRUCTURE_VK_H
#define ACCELERATION_STRUCTURE_VK_H

#include <vulkan/vulkan.hpp>

#include "buffer_vk.h"

#include <cstdint>
#include <vector>

class VulkanMain;

class AccelerationStructureVk
{
public:
	explicit AccelerationStructureVk(VulkanMain& vk);
	~AccelerationStructureVk();

	AccelerationStructureVk(const AccelerationStructureVk&) = delete;
	AccelerationStructureVk& operator=(const AccelerationStructureVk&) = delete;

	AccelerationStructureVk(AccelerationStructureVk&&) noexcept = default;
	AccelerationStructureVk& operator=(AccelerationStructureVk&&) noexcept = default;

	void destroy();

	void buildBLAS(
		vk::Buffer vertexBuffer,
		vk::DeviceAddress vertexAddress,
		uint32_t vertexCount,
		vk::DeviceSize vertexStride,
		vk::Buffer indexBuffer,
		vk::DeviceAddress indexAddress,
		uint32_t indexCount,
		vk::IndexType indexType = vk::IndexType::eUint32
	);

	void buildTLAS(const std::vector<vk::AccelerationStructureInstanceKHR>& instances);

	bool valid() const { return static_cast<bool>(as_); }

	vk::AccelerationStructureKHR handle() const { return as_.get(); }
	vk::DeviceAddress deviceAddress() const { return deviceAddress_; }

private:
	VulkanMain* vk_;

	BufferVk buffer_;
	vk::UniqueAccelerationStructureKHR as_{};
	vk::DeviceAddress deviceAddress_{ 0 };
};

#endif
