#ifndef DESCRIPTOR_SET_VK_H
#define DESCRIPTOR_SET_VK_H

#include <vulkan/vulkan.hpp>

#include <cstdint>

class VulkanMain;

class DescriptorSetVk
{
public:
	explicit DescriptorSetVk(VulkanMain& vk);
	~DescriptorSetVk();

	DescriptorSetVk(const DescriptorSetVk&) = delete;
	DescriptorSetVk& operator=(const DescriptorSetVk&) = delete;

	DescriptorSetVk(DescriptorSetVk&&) noexcept = default;
	DescriptorSetVk& operator=(DescriptorSetVk&&) noexcept = default;

	void createLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings);

	void createPool(const std::vector<vk::DescriptorPoolSize>& poolSizes, uint32_t maxSets = 1);

	void allocate();

	void destroy();

	void writeUniformBuffer(
		uint32_t binding,
		vk::Buffer buffer,
		vk::DeviceSize range,
		vk::DeviceSize offset = 0
	);

	void writeDynamicUniformBuffer(
		uint32_t binding,
		vk::Buffer buffer,
		vk::DeviceSize range
	);

	void writeStorageBuffer(
		uint32_t binding,
		vk::Buffer buffer,
		vk::DeviceSize range,
		vk::DeviceSize offset = 0
	);

	void writeCombinedImageSampler(
		uint32_t binding,
		vk::ImageView imageView,
		vk::Sampler sampler,
		vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
	);

	void createSingleUniformBuffer(
		uint32_t binding,
		vk::ShaderStageFlags stageFlags,
		vk::Buffer buffer,
		vk::DeviceSize range
	);

	void writeStorageImage(
		uint32_t binding,
		vk::ImageView imageView,
		vk::ImageLayout imageLayout = vk::ImageLayout::eGeneral
	);

	void writeAccelerationStructure(
		uint32_t binding,
		vk::AccelerationStructureKHR accel
	);

	bool valid() const 
	{ 
		return static_cast<bool>(setLayout_) && 
			static_cast<bool>(descPool_) && 
			static_cast<bool>(descSet_); 
	} // end of valid()

	vk::DescriptorSetLayout getLayout() const { return setLayout_.get(); };
	vk::DescriptorSet getSet() const { return descSet_; };

private:
	VulkanMain& vk_;

	vk::UniqueDescriptorSetLayout setLayout_{};
	vk::UniqueDescriptorPool descPool_{};
	vk::DescriptorSet descSet_{};

};

#endif
