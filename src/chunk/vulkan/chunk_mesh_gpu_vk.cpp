#include "chunk_mesh_gpu_vk.h"

#include <vulkan/vulkan.hpp>

#include "vulkan_main.h"
#include "chunk_mesh_data.h"

#include <vector>
#include <utility>

using namespace World;

//--- PUBLIC ---//
ChunkMeshGPUVk::ChunkMeshGPUVk(VulkanMain& vk)
	: vk_(&vk),
	opaqueBLAS_(vk),
	opaqueRTVB_(vk),
	opaqueRTIB_(vk),
	opaqueVB_(vk),
	opaqueIB_(vk),
	waterBLAS_(vk),
	waterRTVB_(vk),
	waterRTIB_(vk),
	waterVB_(vk),
	waterIB_(vk)
{
} // end of constructor

ChunkMeshGPUVk::~ChunkMeshGPUVk()
{
	if (vk_)
	{
		const uint32_t frameIndex = vk_->currentFrameIndex();
		retireCurrentBLAS(frameIndex);
		retireCurrentBuffers(frameIndex);
	}
} // end of destructor

void ChunkMeshGPUVk::upload(
	vk::CommandBuffer cmd,
	const ChunkMeshData& data
)
{
	const bool rtEnabled = vk_->supportsRayTracing();

	BufferVk newOpaqueRTVB(*vk_);
	BufferVk newOpaqueRTIB(*vk_);
	BufferVk newOpaqueVB(*vk_);
	BufferVk newOpaqueIB(*vk_);

	BufferVk newWaterRTVB(*vk_);
	BufferVk newWaterRTIB(*vk_);
	BufferVk newWaterVB(*vk_);
	BufferVk newWaterIB(*vk_);

	uint32_t newOpaqueRTVertexCount = 0;
	uint32_t newOpaqueRTIndexCount = 0;
	uint32_t newOpaqueIndexCount = 0;

	uint32_t newWaterRTVertexCount = 0;
	uint32_t newWaterRTIndexCount = 0;
	uint32_t newWaterIndexCount = 0;

	std::vector<BufferVk> stagingBuffers;
	stagingBuffers.reserve(8);

	// copy data to CPU side holders
	if (rtEnabled)
	{
		opaqueRTVerticesCPU_ = data.opaqueRTVertices;
		opaqueRTIndicesCPU_ = data.opaqueIndices;
		waterRTVerticesCPU_ = data.waterRTVertices;
		waterRTIndicesCPU_ = data.waterIndices;
	}
	else
	{
		opaqueRTVerticesCPU_.clear();
		opaqueRTIndicesCPU_.clear();
		waterRTVerticesCPU_.clear();
		waterRTIndicesCPU_.clear();
	}

	// -------- RT OPAQUE --------
	if (rtEnabled && !data.opaqueRTVertices.empty() && !data.opaqueIndices.empty())
	{
		vk::DeviceSize vbSize = sizeof(RTVertex) * data.opaqueRTVertices.size();
		vk::DeviceSize ibSize = sizeof(uint32_t) * data.opaqueIndices.size();

		// RT VB staging
		BufferVk stagingVB(*vk_);
		stagingVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingVB.upload(data.opaqueRTVertices.data(), vbSize);

		// RT VB device local
		newOpaqueRTVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferDst |
			vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eStorageBuffer |
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			true
		);
		stagingBuffers.push_back(std::move(stagingVB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newOpaqueRTVB.getBuffer(),
			vbSize
		);

		// RT IB staging
		BufferVk stagingIB(*vk_);
		stagingIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingIB.upload(data.opaqueIndices.data(), ibSize);

		// RT IB device local
		newOpaqueRTIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferDst |
			vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eStorageBuffer |
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			true
		);
		stagingBuffers.push_back(std::move(stagingIB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newOpaqueRTIB.getBuffer(),
			ibSize
		);

		newOpaqueRTIndexCount = static_cast<uint32_t>(data.opaqueIndices.size());
		newOpaqueRTVertexCount = static_cast<uint32_t>(data.opaqueRTVertices.size());
	}

	// -------- OPAQUE --------
	if (!data.opaqueVertices.empty() && !data.opaqueIndices.empty())
	{
		vk::DeviceSize vbSize = sizeof(Vertex) * data.opaqueVertices.size();
		vk::DeviceSize ibSize = sizeof(uint32_t) * data.opaqueIndices.size();

		// VB staging
		BufferVk stagingVB(*vk_);
		stagingVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingVB.upload(data.opaqueVertices.data(), vbSize);

		// VB device local
		newOpaqueVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		stagingBuffers.push_back(std::move(stagingVB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newOpaqueVB.getBuffer(),
			vbSize
		);

		// IB staging
		BufferVk stagingIB(*vk_);
		stagingIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingIB.upload(data.opaqueIndices.data(), ibSize);

		// IB device local
		newOpaqueIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		stagingBuffers.push_back(std::move(stagingIB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newOpaqueIB.getBuffer(), 
			ibSize
		);

		newOpaqueIndexCount = static_cast<uint32_t>(data.opaqueIndices.size());
	}


	// -------- RT WATER --------
	if (rtEnabled && !data.waterRTVertices.empty() && !data.waterIndices.empty())
	{
		vk::DeviceSize vbSize = sizeof(RTVertex) * data.waterRTVertices.size();
		vk::DeviceSize ibSize = sizeof(uint32_t) * data.waterIndices.size();

		// RT VB staging
		BufferVk stagingVB(*vk_);
		stagingVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingVB.upload(data.waterRTVertices.data(), vbSize);

		// RT VB device local
		newWaterRTVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferDst |
			vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eStorageBuffer |
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			true
		);
		stagingBuffers.push_back(std::move(stagingVB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newWaterRTVB.getBuffer(),
			vbSize
		);

		// RT IB staging
		BufferVk stagingIB(*vk_);
		stagingIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingIB.upload(data.waterIndices.data(), ibSize);

		// RT IB device local
		newWaterRTIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferDst |
			vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eStorageBuffer |
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			true
		);
		stagingBuffers.push_back(std::move(stagingIB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newWaterRTIB.getBuffer(),
			ibSize
		);

		newWaterRTIndexCount = static_cast<uint32_t>(data.waterIndices.size());
		newWaterRTVertexCount = static_cast<uint32_t>(data.waterRTVertices.size());
	}

	// -------- WATER --------
	if (!data.waterVertices.empty() && !data.waterIndices.empty())
	{
		vk::DeviceSize vbSize = sizeof(VertexWater) * data.waterVertices.size();
		vk::DeviceSize ibSize = sizeof(uint32_t) * data.waterIndices.size();

		// VB staging
		BufferVk stagingVB(*vk_);
		stagingVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingVB.upload(data.waterVertices.data(), vbSize);

		// VB device local
		newWaterVB.create(
			vbSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		stagingBuffers.push_back(std::move(stagingVB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newWaterVB.getBuffer(),
			vbSize
		);

		// IB staging
		BufferVk stagingIB(*vk_);
		stagingIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		stagingIB.upload(data.waterIndices.data(), ibSize);

		// IB device local
		newWaterIB.create(
			ibSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		stagingBuffers.push_back(std::move(stagingIB));
		vk_->recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			newWaterIB.getBuffer(),
			ibSize
		);

		newWaterIndexCount = static_cast<uint32_t>(data.waterIndices.size());
	}

	const uint32_t frameIndex = vk_->currentFrameIndex();
	if (rtEnabled)
	{
		retireCurrentBLAS(frameIndex);
	}
	retireCurrentBuffers(frameIndex);

	opaqueRTVB_ = std::move(newOpaqueRTVB);
	opaqueRTIB_ = std::move(newOpaqueRTIB);
	opaqueVB_ = std::move(newOpaqueVB);
	opaqueIB_ = std::move(newOpaqueIB);

	waterRTVB_ = std::move(newWaterRTVB);
	waterRTIB_ = std::move(newWaterRTIB);
	waterVB_ = std::move(newWaterVB);
	waterIB_ = std::move(newWaterIB);

	opaqueRTVertexCount_ = newOpaqueRTVertexCount;
	opaqueRTIndexCount_ = newOpaqueRTIndexCount;
	opaqueIndexCount_ = newOpaqueIndexCount;

	waterRTVertexCount_ = newWaterRTVertexCount;
	waterRTIndexCount_ = newWaterRTIndexCount;
	waterIndexCount_ = newWaterIndexCount;

	if (rtEnabled)
	{
		std::vector<vk::BufferMemoryBarrier> barriers;

		auto addBarrier = [&](const BufferVk& buffer)
			{
				if (!buffer.valid())
					return;

				vk::BufferMemoryBarrier b{};
				b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				b.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR;
				b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				b.buffer = buffer.getBuffer();
				b.offset = 0;
				b.size = VK_WHOLE_SIZE;

				barriers.push_back(b);
			};

		addBarrier(opaqueRTVB_);
		addBarrier(opaqueRTIB_);
		addBarrier(waterRTVB_);
		addBarrier(waterRTIB_);

		if (!barriers.empty())
		{
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
				{},
				0, nullptr,
				static_cast<uint32_t>(barriers.size()),
				barriers.data(),
				0, nullptr
			);
		}

		// build opaque BLAS
		if (opaqueRTVB_.valid() && opaqueRTIB_.valid() &&
			opaqueRTVertexCount_ > 0 && opaqueRTIndexCount_ > 0)
		{
			opaqueBLAS_.buildBLASOnCmd(
				cmd,
				opaqueRTVB_.getBuffer(),
				opaqueRTVB_.getDeviceAddress(),
				opaqueRTVertexCount_,
				sizeof(RTVertex),
				opaqueRTIB_.getBuffer(),
				opaqueRTIB_.getDeviceAddress(),
				opaqueRTIndexCount_,
				vk::IndexType::eUint32
			);
		}
		// build water BLAS
		if (waterRTVB_.valid() && waterRTIB_.valid() &&
			waterRTVertexCount_ > 0 && waterRTIndexCount_ > 0)
		{
			waterBLAS_.buildBLASOnCmd(
				cmd,
				waterRTVB_.getBuffer(),
				waterRTVB_.getDeviceAddress(),
				waterRTVertexCount_,
				sizeof(RTVertex),
				waterRTIB_.getBuffer(),
				waterRTIB_.getDeviceAddress(),
				waterRTIndexCount_,
				vk::IndexType::eUint32
			);
		}
	}

	for (auto& staging : stagingBuffers)
	{
		vk_->retireBuffer(frameIndex, std::move(staging));
	} // end for

	stagingBuffers.clear();
} // end of upload()

void ChunkMeshGPUVk::drawOpaque(vk::CommandBuffer cmd)
{
	if (!cmd || opaqueIndexCount_ == 0 || !opaqueVB_.valid() || !opaqueIB_.valid())
		return;

	vk::Buffer vb = opaqueVB_.getBuffer();
	vk::DeviceSize offset = 0;

	cmd.bindVertexBuffers(0, 1, &vb, &offset);
	cmd.bindIndexBuffer(opaqueIB_.getBuffer(), 0, vk::IndexType::eUint32);
	cmd.drawIndexed(opaqueIndexCount_, 1, 0, 0, 0);
} // end of drawOpaque()

void ChunkMeshGPUVk::drawWater(vk::CommandBuffer cmd)
{
	if (!cmd || waterIndexCount_ == 0 || !waterVB_.valid() || !waterIB_.valid())
		return;

	vk::Buffer vb = waterVB_.getBuffer();
	vk::DeviceSize offset = 0;

	cmd.bindVertexBuffers(0, 1, &vb, &offset);
	cmd.bindIndexBuffer(waterIB_.getBuffer(), 0, vk::IndexType::eUint32);
	cmd.drawIndexed(waterIndexCount_, 1, 0, 0, 0);
} // end of drawWater()


//--- PRIVATE ---//
void ChunkMeshGPUVk::retireCurrentBuffers(uint32_t frameIndex)
{
	if (!opaqueRTVB_.valid() && !opaqueRTIB_.valid() &&
		!opaqueVB_.valid() && !opaqueIB_.valid() &&
		!waterRTVB_.valid() && !waterRTIB_.valid() &&
		!waterVB_.valid() && !waterIB_.valid())
	{
		return;
	}

	vk_->retireBuffer(frameIndex, std::move(opaqueRTVB_));
	vk_->retireBuffer(frameIndex, std::move(opaqueRTIB_));
	vk_->retireBuffer(frameIndex, std::move(opaqueVB_));
	vk_->retireBuffer(frameIndex, std::move(opaqueIB_));

	vk_->retireBuffer(frameIndex, std::move(waterRTVB_));
	vk_->retireBuffer(frameIndex, std::move(waterRTIB_));
	vk_->retireBuffer(frameIndex, std::move(waterVB_));
	vk_->retireBuffer(frameIndex, std::move(waterIB_));
} // end of retireCurrentBuffers()

void ChunkMeshGPUVk::retireCurrentBLAS(uint32_t frameIndex)
{
	if (opaqueBLAS_.valid())
	{
		vk_->retireAccelerationStructure(frameIndex, std::move(opaqueBLAS_));
		opaqueBLAS_ = AccelerationStructureVk(*vk_);
	}
	if (waterBLAS_.valid())
	{
		vk_->retireAccelerationStructure(frameIndex, std::move(waterBLAS_));
		waterBLAS_ = AccelerationStructureVk(*vk_);
	}
} // end of retireCurrentBLAS()
