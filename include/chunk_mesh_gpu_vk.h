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

	void upload(const ChunkMeshData& data) override;
	void drawOpaque(vk::CommandBuffer cmd) override;
	void drawWater(vk::CommandBuffer cmd) override;

	const std::vector<World::RTVertex>& getOpaqueRTVerticesCPU() const { return opaqueRTVerticesCPU_; }
	const std::vector<uint32_t>& getOpaqueRTIndicesCPU() const { return opaqueRTIndicesCPU_; }

	vk::Buffer getOpaqueRTVertexBuffer() const { return opaqueRTVB_.getBuffer(); }
	vk::Buffer getOpaqueRTIndexBuffer() const { return opaqueRTIB_.getBuffer(); }

	vk::DeviceAddress getOpaqueRTVertexAddress() const { return opaqueRTVB_.getDeviceAddress(); }
	vk::DeviceAddress getOpaqueRTIndexAddress() const { return opaqueRTIB_.getDeviceAddress(); }

	uint32_t getOpaqueRTIndexCount() const { return opaqueRTIndexCount_; }
	uint32_t getOpaqueRTVertexCount() const { return opaqueRTVertexCount_; }

	// BLAS
	vk::AccelerationStructureKHR getOpaqueBLAS() const { return opaqueBLAS_.handle(); }
	vk::DeviceAddress getOpaqueBLASAddress() const { return opaqueBLAS_.deviceAddress(); }
	bool hasOpaqueBLAS() const { return opaqueBLAS_.valid(); }

private:
	void retireCurrentBuffers(uint32_t frameIndex);
	void retireCurrentBLAS(uint32_t frameIndex);
private:
	VulkanMain* vk_{};

	AccelerationStructureVk opaqueBLAS_;

	// RT opaque
	BufferVk opaqueRTVB_;
	BufferVk opaqueRTIB_;
	uint32_t opaqueRTIndexCount_{ 0 };
	uint32_t opaqueRTVertexCount_{ 0 };

	std::vector<World::RTVertex> opaqueRTVerticesCPU_;
	std::vector<uint32_t> opaqueRTIndicesCPU_;

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
