#ifndef CHUNK_MESH_GPU_VK_H
#define CHUNK_MESH_GPU_VK_H

#include "constants.h"

#include "i_chunk_mesh_gpu.h"

#include "acceleration_structure_vk.h"
#include "buffer_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <vector>

class VulkanMain;
struct ChunkMeshData;

class ChunkMeshGPUVk final : public IChunkMeshGPU
{
public:
	explicit ChunkMeshGPUVk(VulkanMain& vk);
	~ChunkMeshGPUVk() override;

	void upload(
		const ChunkMeshData& data,
		const FrameContext* frameVk = nullptr,
		const FrameContextDX12* frameDX12 = nullptr
	) override;
	void drawOpaque(
		const FrameContext* frameVk = nullptr,
		const FrameContextDX12* frameDX12 = nullptr
	) override;
	void drawWater(
		const FrameContext* frameVk = nullptr,
		const FrameContextDX12* frameDX12 = nullptr
	) override;

	const std::vector<World::RTVertex>& getOpaqueRTVerticesCPU() const { return opaqueRTVerticesCPU_; }
	const std::vector<uint32_t>& getOpaqueRTIndicesCPU() const { return opaqueRTIndicesCPU_; }
	vk::DeviceAddress getOpaqueRTVertexAddress() const { return opaqueRTVB_.getDeviceAddress(); }
	vk::DeviceAddress getOpaqueRTIndexAddress() const { return opaqueRTIB_.getDeviceAddress(); }

	const std::vector<World::RTVertex>& getWaterRTVerticesCPU() const { return waterRTVerticesCPU_; }
	const std::vector<uint32_t>& getWaterRTIndicesCPU() const { return waterRTIndicesCPU_; }
	vk::DeviceAddress getWaterRTVertexAddress() const { return waterRTVB_.getDeviceAddress(); }
	vk::DeviceAddress getWaterRTIndexAddress() const { return waterRTIB_.getDeviceAddress(); }

	const AccelerationStructureVk& getOpaqueBLAS() const { return opaqueBLAS_; }
	const AccelerationStructureVk& getWaterBLAS() const { return waterBLAS_; }

private:
	void retireCurrentBuffers(uint32_t frameIndex);
	void retireCurrentBLAS(uint32_t frameIndex);
private:
	VulkanMain* vk_{ nullptr };

	AccelerationStructureVk opaqueBLAS_;
	AccelerationStructureVk waterBLAS_;

	// RT opaque
	BufferVk opaqueRTVB_;
	BufferVk opaqueRTIB_;
	uint32_t opaqueRTIndexCount_{ 0 };
	uint32_t opaqueRTVertexCount_{ 0 };

	std::vector<World::RTVertex> opaqueRTVerticesCPU_;
	std::vector<uint32_t> opaqueRTIndicesCPU_;

	// RT water
	BufferVk waterRTVB_;
	BufferVk waterRTIB_;
	uint32_t waterRTIndexCount_{ 0 };
	uint32_t waterRTVertexCount_{ 0 };

	std::vector<World::RTVertex> waterRTVerticesCPU_;
	std::vector<uint32_t> waterRTIndicesCPU_;

	// opaque
	BufferVk opaqueVB_;
	BufferVk opaqueIB_;
	uint32_t opaqueIndexCount_{ 0 };

	// water
	BufferVk waterVB_;
	BufferVk waterIB_;
	uint32_t waterIndexCount_{ 0 };
};

#endif
