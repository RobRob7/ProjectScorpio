#include "timestamp_gpu_vk.h"

#include "vulkan_main.h"

#include <stdexcept>
#include <utility>

//--- PUBLIC ---//
void TimestampGPUVk::create(vk::Device& device)
{
	vk::QueryPoolCreateInfo createInfo{};
	createInfo.queryType = vk::QueryType::eTimestamp;
	createInfo.queryCount = static_cast<uint32_t>(GPUPass::COUNT) * 2;

	{
		vk::ResultValue rv = device.createQueryPoolUnique(createInfo);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("TimestampGPUVk::createQueryPoolUnique failed: " + vk::to_string(rv.result));
		}
		queryPool_ = std::move(rv.value);
	}
} // end of create()

void TimestampGPUVk::reset(vk::CommandBuffer cmd)
{
	cmd.resetQueryPool(
		queryPool_.get(),
		0,
		static_cast<uint32_t>(GPUPass::COUNT) * 2
	);
} // end of reset

void TimestampGPUVk::beginPass(
	vk::CommandBuffer cmd, 
	GPUPass pass
)
{
	uint32_t queryIndex = static_cast<uint32_t>(pass) * 2;

	cmd.writeTimestamp2(
		vk::PipelineStageFlagBits2::eTopOfPipe,
		*queryPool_,
		queryIndex
	);
} // end of beginPass()

void TimestampGPUVk::endPass(
	vk::CommandBuffer cmd, 
	GPUPass pass
)
{
	uint32_t queryIndex = (static_cast<uint32_t>(pass) * 2) + 1;

	cmd.writeTimestamp2(
		vk::PipelineStageFlagBits2::eBottomOfPipe,
		*queryPool_,
		queryIndex
	);
} // end of endPass()

double TimestampGPUVk::getPassTimeMs(
	vk::Device& device,
	GPUPass pass,
	float timePeriod
)
{
	constexpr uint32_t queryCount = static_cast<uint32_t>(GPUPass::COUNT) * 2;

	vk::Result res = device.getQueryPoolResults(
		*queryPool_,
		0,
		queryCount,
		sizeof(uint64_t) * timestamps_.size(),
		timestamps_.data(),
		sizeof(uint64_t),
		vk::QueryResultFlagBits::e64 |
		vk::QueryResultFlagBits::eWait
	);

	if (res != vk::Result::eSuccess)
	{
		return 0.0;
	}

	uint32_t base = static_cast<uint32_t>(pass) * 2;

	uint64_t start = timestamps_[base];
	uint64_t end = timestamps_[base + 1];

	return double(end - start) * double(timePeriod) / 1'000'000.0;
} // end of getPassTimeMs()