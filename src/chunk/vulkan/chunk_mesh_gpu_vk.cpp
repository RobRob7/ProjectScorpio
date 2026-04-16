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
	waterVB_(vk),
	waterIB_(vk)
{
} // end of constructor

ChunkMeshGPUVk::~ChunkMeshGPUVk()
{
	if (vk_)
	{
		retireCurrentBuffers(vk_->currentFrameIndex());
	}
} // end of destructor

void ChunkMeshGPUVk::upload(const ChunkMeshData& data)
{
	BufferVk newOpaqueRTVB(*vk_);
	BufferVk newOpaqueRTIB(*vk_);
	BufferVk newOpaqueVB(*vk_);
	BufferVk newOpaqueIB(*vk_);
	BufferVk newWaterVB(*vk_);
	BufferVk newWaterIB(*vk_);

	uint32_t newOpaqueRTVertexCount = 0;
	uint32_t newOpaqueRTIndexCount = 0;
	uint32_t newOpaqueIndexCount = 0;
	uint32_t newWaterIndexCount = 0;

	vk::CommandBuffer cmd = vk_->beginSingleTimeCommands();
	std::vector<BufferVk> stagingBuffers;
	stagingBuffers.reserve(6);

	// -------- RT OPAQUE --------
	if (!data.opaqueRTVertices.empty() && !data.opaqueIndices.empty())
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

	retireCurrentBuffers(vk_->currentFrameIndex());

	opaqueRTVB_ = std::move(newOpaqueRTVB);
	opaqueRTIB_ = std::move(newOpaqueRTIB);
	opaqueVB_ = std::move(newOpaqueVB);
	opaqueIB_ = std::move(newOpaqueIB);
	waterVB_ = std::move(newWaterVB);
	waterIB_ = std::move(newWaterIB);

	opaqueRTVertexCount_ = newOpaqueRTVertexCount;
	opaqueRTIndexCount_ = newOpaqueRTIndexCount;
	opaqueIndexCount_ = newOpaqueIndexCount;
	waterIndexCount_ = newWaterIndexCount;

	if (!stagingBuffers.empty())
	{
		vk_->submitUpload(cmd, std::move(stagingBuffers));
	}
	else
	{
		vk_->discardSingleTimeCommands(cmd);
	}

	// retire old BLAS
	if (opaqueBLAS_.valid())
	{
		vk_->retireAccelerationStructure(
			vk_->currentFrameIndex(),
			std::move(opaqueBLAS_)
		);
		opaqueBLAS_ = AccelerationStructureVk(*vk_);
	}

	// build BLAS
	if (opaqueRTVB_.valid() && opaqueRTIB_.valid() &&
		opaqueRTVertexCount_ > 0 && opaqueRTIndexCount_ > 0)
	{
		opaqueBLAS_.buildBLAS(
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
		!waterVB_.valid() && !waterIB_.valid())
	{
		return;
	}

	vk_->retireChunkBuffers(
		frameIndex,
		std::move(opaqueRTVB_),
		std::move(opaqueRTIB_),
		std::move(opaqueVB_),
		std::move(opaqueIB_),
		std::move(waterVB_),
		std::move(waterIB_)
	);
} // end of retireCurrentBuffers()
