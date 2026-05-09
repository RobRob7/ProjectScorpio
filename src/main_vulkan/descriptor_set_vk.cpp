#include "descriptor_set_vk.h"

#include "vulkan_main.h"

#include <vulkan/vulkan.hpp>

#include <stdexcept>
#include <cstdint>

//--- PUBLIC ---//
DescriptorSetVk::DescriptorSetVk(VulkanMain& vk)
	: vk_(vk)
{
} // end of constructor

DescriptorSetVk::~DescriptorSetVk() = default;

void DescriptorSetVk::setDebugName(const std::string& name)
{
	debugName_ = name;

	if (!descSet_) return;

	vk::DebugUtilsObjectNameInfoEXT info{};
	info.objectType = vk::ObjectType::eDescriptorSet;
	info.objectHandle = reinterpret_cast<uint64_t>(
		static_cast<VkDescriptorSet>(descSet_)
		);
	info.pObjectName = debugName_.c_str();

	vk_.getDevice().setDebugUtilsObjectNameEXT(info);
} // end of setDebugName()

void DescriptorSetVk::createLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings)
{
	if (bindings.empty())
	{
		throw std::runtime_error("DescriptorSetVk::createLayout - bindings cannot be empty");
	}

	vk::Device device = vk_.getDevice();

	vk::DescriptorSetLayoutCreateInfo slci{};
	slci.bindingCount = static_cast<uint32_t>(bindings.size());
	slci.pBindings = bindings.data();

	vk::ResultValue rv = device.createDescriptorSetLayoutUnique(slci);
	if (rv.result != vk::Result::eSuccess)
	{
		throw std::runtime_error(
			"DescriptorSetVk::createLayout - createDescriptorSetLayoutUnique failed: " +
			vk::to_string(rv.result)
		);
	}

	setLayout_ = std::move(rv.value);
} // end of createLayout()

void DescriptorSetVk::createPool(const std::vector<vk::DescriptorPoolSize>& poolSizes, uint32_t maxSets)
{
	if (poolSizes.empty())
	{
		throw std::runtime_error("DescriptorSetVk::createPool - poolSizes cannot be empty");
	}

	vk::Device device = vk_.getDevice();

	vk::DescriptorPoolCreateInfo dpci{};
	dpci.maxSets = maxSets;
	dpci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	dpci.pPoolSizes = poolSizes.data();

	vk::ResultValue rv = device.createDescriptorPoolUnique(dpci);
	if (rv.result != vk::Result::eSuccess)
	{
		throw std::runtime_error(
			"DescriptorSetVk::createPool - createDescriptorPoolUnique failed: " +
			vk::to_string(rv.result)
		);
	}

	descPool_ = std::move(rv.value);
} // end of createPool()

void DescriptorSetVk::allocate()
{
	if (!setLayout_)
	{
		throw std::runtime_error("DescriptorSetVk::allocate - descriptor set layout not created");
	}

	if (!descPool_)
	{
		throw std::runtime_error("DescriptorSetVk::allocate - descriptor pool not created");
	}

	vk::Device device = vk_.getDevice();

	vk::DescriptorSetLayout layout = setLayout_.get();

	vk::DescriptorSetAllocateInfo dsai{};
	dsai.descriptorPool = descPool_.get();
	dsai.descriptorSetCount = 1;
	dsai.pSetLayouts = &layout;

	vk::ResultValue rv = device.allocateDescriptorSets(dsai);
	if (rv.result != vk::Result::eSuccess)
	{
		throw std::runtime_error(
			"DescriptorSetVk::allocate - allocateDescriptorSets failed: " +
			vk::to_string(rv.result)
		);
	}

	descSet_ = rv.value[0];
} // end of allocate()

void DescriptorSetVk::destroy()
{
	setLayout_.reset();
	descPool_.reset();
	descSet_ = vk::DescriptorSet{};
} // end of destroy()

void DescriptorSetVk::writeUniformBuffer(
	uint32_t binding,
	vk::Buffer buffer,
	vk::DeviceSize range,
	vk::DeviceSize offset
)
{
	if (!descSet_)
	{
		throw std::runtime_error("DescriptorSetVk::writeUniformBuffer - descriptor set not allocated");
	}

	vk::DescriptorBufferInfo dbi{};
	dbi.buffer = buffer;
	dbi.offset = offset;
	dbi.range = range;

	vk::WriteDescriptorSet write{};
	write.dstSet = descSet_;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = vk::DescriptorType::eUniformBuffer;
	write.descriptorCount = 1;
	write.pBufferInfo = &dbi;

	vk_.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
} // end of writeUniformBuffer()

void DescriptorSetVk::writeDynamicUniformBuffer(
	uint32_t binding,
	vk::Buffer buffer,
	vk::DeviceSize range
)
{
	vk::DescriptorBufferInfo info{};
	info.buffer = buffer;
	info.offset = 0;
	info.range = range;

	vk::WriteDescriptorSet write{};
	write.dstSet = descSet_;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
	write.pBufferInfo = &info;

	vk_.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
} // end of writeDynamicUniformBuffer()

void DescriptorSetVk::writeStorageBuffer(
	uint32_t binding,
	vk::Buffer buffer,
	vk::DeviceSize range,
	vk::DeviceSize offset
)
{
	if (!descSet_)
	{
		throw std::runtime_error("DescriptorSetVk::writeStorageBuffer - descriptor set not allocated");
	}

	vk::DescriptorBufferInfo dbi{};
	dbi.buffer = buffer;
	dbi.offset = offset;
	dbi.range = range;

	vk::WriteDescriptorSet write{};
	write.dstSet = descSet_;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = vk::DescriptorType::eStorageBuffer;
	write.descriptorCount = 1;
	write.pBufferInfo = &dbi;

	vk_.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
} // end of writeStorageBuffer()

void DescriptorSetVk::writeCombinedImageSampler(
	uint32_t binding,
	vk::ImageView imageView,
	vk::Sampler sampler,
	vk::ImageLayout imageLayout
)
{
	if (!descSet_)
	{
		throw std::runtime_error("DescriptorSetVk::writeCombinedImageSampler - descriptor set not allocated");
	}

	vk::DescriptorImageInfo dii{};
	dii.imageLayout = imageLayout;
	dii.imageView = imageView;
	dii.sampler = sampler;

	vk::WriteDescriptorSet write{};
	write.dstSet = descSet_;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	write.descriptorCount = 1;
	write.pImageInfo = &dii;

	vk_.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
} // end of writeCombinedImageSampler()


void DescriptorSetVk::createSingleUniformBuffer(
	uint32_t binding,
	vk::ShaderStageFlags stageFlags,
	vk::Buffer buffer,
	vk::DeviceSize range
)
{
	destroy();

	vk::Device device = vk_.getDevice();

	// layout
	vk::DescriptorSetLayoutBinding uboBinding{};
	uboBinding.binding = binding;
	uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	uboBinding.descriptorCount = 1;
	uboBinding.stageFlags = stageFlags;

	vk::DescriptorSetLayoutCreateInfo slci{};
	slci.bindingCount = 1;
	slci.pBindings = &uboBinding;

	{
		vk::ResultValue rv = device.createDescriptorSetLayoutUnique(slci);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"DescriptorSetVk::createSingleUniformBuffer - createDescriptorSetLayoutUnique failed: " +
				vk::to_string(rv.result)
			);
		}
		setLayout_ = std::move(rv.value);
	}

	// pool
	vk::DescriptorPoolSize poolSize{};
	poolSize.type = vk::DescriptorType::eUniformBuffer;
	poolSize.descriptorCount = 1;

	vk::DescriptorPoolCreateInfo dpci{};
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &poolSize;

	{
		vk::ResultValue rv = device.createDescriptorPoolUnique(dpci);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"DescriptorSetVk::createSingleUniformBuffer - createDescriptorPoolUnique failed: " +
				vk::to_string(rv.result)
			);
		}
		descPool_ = std::move(rv.value);
	}

	// allocate set
	vk::DescriptorSetAllocateInfo dsai{};
	dsai.descriptorPool = descPool_.get();
	dsai.descriptorSetCount = 1;

	vk::DescriptorSetLayout layoutHandle = setLayout_.get();
	dsai.pSetLayouts = &layoutHandle;

	{
		vk::ResultValue rv = device.allocateDescriptorSets(dsai);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"DescriptorSetVk::createSingleUniformBuffer - allocateDescriptorSets failed: " +
				vk::to_string(rv.result)
			);
		}
		descSet_ = rv.value[0];
	}

	// write descriptor
	vk::DescriptorBufferInfo dbi{};
	dbi.buffer = buffer;
	dbi.offset = 0;
	dbi.range = range;

	vk::WriteDescriptorSet write{};
	write.dstSet = descSet_;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = vk::DescriptorType::eUniformBuffer;
	write.descriptorCount = 1;
	write.pBufferInfo = &dbi;

	device.updateDescriptorSets(1, &write, 0, nullptr);
} // end of createSingleUniformBuffer()

void DescriptorSetVk::writeStorageImage(
	uint32_t binding,
	vk::ImageView imageView,
	vk::ImageLayout imageLayout
)
{
	if (!descSet_)
	{
		throw std::runtime_error("DescriptorSetVk::writeStorageImage - descriptor set not allocated");
	}

	vk::DescriptorImageInfo dii{};
	dii.imageLayout = imageLayout;
	dii.imageView = imageView;
	dii.sampler = vk::Sampler{};

	vk::WriteDescriptorSet write{};
	write.dstSet = descSet_;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = vk::DescriptorType::eStorageImage;
	write.descriptorCount = 1;
	write.pImageInfo = &dii;

	vk_.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
} // end of writeStorageImage()

void DescriptorSetVk::writeAccelerationStructure(
	uint32_t binding,
	vk::AccelerationStructureKHR accel
)
{
	if (!descSet_)
	{
		throw std::runtime_error("DescriptorSetVk::writeAccelerationStructure - descriptor set not allocated");
	}

	vk::WriteDescriptorSetAccelerationStructureKHR asInfo{};
	asInfo.accelerationStructureCount = 1;
	asInfo.pAccelerationStructures = &accel;

	vk::WriteDescriptorSet write{};
	write.pNext = &asInfo;
	write.dstSet = descSet_;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
	write.descriptorCount = 1;

	vk_.getDevice().updateDescriptorSets(1, &write, 0, nullptr);
} // end of writeAccelerationStructure()
