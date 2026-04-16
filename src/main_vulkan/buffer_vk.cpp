#include "buffer_vk.h"

#include "vulkan_main.h"

#include <vulkan/vulkan.hpp>

#include <stdexcept>
#include <utility>

//--- PUBLIC ---//
BufferVk::BufferVk(VulkanMain& vk)
	: vk_(&vk)
{
} // end of constructor

BufferVk::~BufferVk() = default;

void BufferVk::create(
	vk::DeviceSize size,
	vk::BufferUsageFlags usage,
	vk::MemoryPropertyFlags properties,
	bool enableDeviceAddress
)
{
	destroy();

	if (size == 0)
	{
		throw std::runtime_error("BufferVk::create - size must be greater than 0");
	}

	deviceAddressEnabled_ = enableDeviceAddress;

	size_ = size;
	properties_ = properties;

	vk::Device device = vk_->getDevice();

	vk::BufferCreateInfo bci{};
	bci.size = size_;
	bci.usage = usage;
	bci.sharingMode = vk::SharingMode::eExclusive;

	{
		vk::ResultValue rv = device.createBufferUnique(bci);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("BufferVk::create - createBufferUnique failed: " + vk::to_string(rv.result));
		}
		buffer_ = std::move(rv.value);
	}

	vk::MemoryRequirements req = device.getBufferMemoryRequirements(buffer_.get());

	vk::MemoryAllocateInfo mai{};
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_->findMemoryType(req.memoryTypeBits, properties);

	vk::MemoryAllocateFlagsInfo flagsInfo{};
	if (enableDeviceAddress)
	{
		flagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
		mai.pNext = &flagsInfo;
	}

	{
		vk::ResultValue rv = device.allocateMemoryUnique(mai);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("BufferVk::create - allocateMemoryUnique failed: " + vk::to_string(rv.result));
		}
		memory_ = std::move(rv.value);
	}

	{
		vk::Result res = device.bindBufferMemory(buffer_.get(), memory_.get(), 0);
		if (res != vk::Result::eSuccess)
		{
			throw std::runtime_error("BufferVk::create - bindBufferMemory failed: " + vk::to_string(res));
		}
	}
} // end of create()

void BufferVk::destroy()
{
	memory_.reset();
	buffer_.reset();
	size_ = 0;
	properties_ = {};
	deviceAddressEnabled_ = false;
} // end of destroy

void BufferVk::upload(const void* data, vk::DeviceSize size, vk::DeviceSize offset)
{
	if (!buffer_ || !memory_)
	{
		throw std::runtime_error("BufferVk::upload - buffer not created");
	}

	if (!data)
	{
		throw std::runtime_error("BufferVk::upload - data is null");
	}

	if (!(properties_ & vk::MemoryPropertyFlagBits::eHostVisible))
	{
		throw std::runtime_error("BufferVk::upload - buffer memory is not host visible");
	}

	if (offset + size > size_)
	{
		throw std::runtime_error("BufferVk::upload - write exceeds buffer size");
	}

	vk::Device device = vk_->getDevice();

	vk::ResultValue rv = device.mapMemory(memory_.get(), offset, size);
	if (rv.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("BufferVk::upload - mapMemory failed: " + vk::to_string(rv.result));
	}

	std::memcpy(rv.value, data, static_cast<std::size_t>(size));

	// if memory is not host-coherent, flush manually
	if (!(properties_ & vk::MemoryPropertyFlagBits::eHostCoherent))
	{
		vk::MappedMemoryRange range{};
		range.memory = memory_.get();
		range.offset = offset;
		range.size = size;

		vk::Result flushRes = device.flushMappedMemoryRanges(1, &range);
		if (flushRes != vk::Result::eSuccess)
		{
			device.unmapMemory(memory_.get());
			throw std::runtime_error("BufferVk::upload - flushMappedMemoryRanges failed: " + vk::to_string(flushRes));
		}
	}

	device.unmapMemory(memory_.get());
} // end of upload()

vk::DeviceAddress BufferVk::getDeviceAddress() const
{
	if (!buffer_)
	{
		throw std::runtime_error("BufferVk::getDeviceAddress - buffer not created yet!");
	}

	if (!deviceAddressEnabled_)
	{
		throw std::runtime_error("BufferVk::getDeviceAddress - device address not enabled for this buffer!");
	}

	vk::BufferDeviceAddressInfo info{};
	info.buffer = buffer_.get();

	return vk_->getDevice().getBufferAddress(info);
} // end of getDeviceAddress()
