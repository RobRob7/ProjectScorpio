#ifndef TIMESTAMP_GPU_VK_H
#define TIMESTAMP_GPU_VK_H

#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>

class VulkanMain;

enum class GPUPass : uint32_t
{
	RTPass,
	COUNT
};

struct TimestampData
{
	GPUPass pass;
	double time = 0.0;
};

class TimestampGPUVk
{
public:
	void create(vk::Device& device);

	void reset(vk::CommandBuffer cmd);

	void beginPass(
		vk::CommandBuffer cmd, 
		GPUPass pass
	);
	void endPass(
		vk::CommandBuffer cmd, 
		GPUPass pass
	);

	double getPassTimeMs(
		vk::Device& device,
		GPUPass pass,
		float timePeriod
	);


private:
	vk::UniqueQueryPool queryPool_;
	std::array<double, static_cast<uint32_t>(GPUPass::COUNT) * 2> timestamps_{};
};

#endif