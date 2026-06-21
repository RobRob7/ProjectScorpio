#ifndef CHUNK_MESH_GPU_DX12_H
#define CHUNK_MESH_GPU_DX12_H

#include "constants.h"

#include "i_chunk_mesh_gpu.h"

//#include "acceleration_structure_vk.h"
#include "buffer_dx12.h"

#include <cstdint>
#include <vector>

class DX12Main;
struct ChunkMeshData;
struct FrameContext;
struct FrameContextDX12;

class ChunkMeshGPUDX12 final : public IChunkMeshGPU
{
public:
	explicit ChunkMeshGPUDX12(DX12Main& dx);
	~ChunkMeshGPUDX12() override;

	void upload(
		const ChunkMeshData& data,
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12
	) override;
	void drawOpaque(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12
	) override;
	void drawWater(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12
	) override;

	//const std::vector<World::RTVertex>& getOpaqueRTVerticesCPU() const { return opaqueRTVerticesCPU_; }
	//const std::vector<uint32_t>& getOpaqueRTIndicesCPU() const { return opaqueRTIndicesCPU_; }
	//vk::DeviceAddress getOpaqueRTVertexAddress() const { return opaqueRTVB_.getDeviceAddress(); }
	//vk::DeviceAddress getOpaqueRTIndexAddress() const { return opaqueRTIB_.getDeviceAddress(); }

	//const std::vector<World::RTVertex>& getWaterRTVerticesCPU() const { return waterRTVerticesCPU_; }
	//const std::vector<uint32_t>& getWaterRTIndicesCPU() const { return waterRTIndicesCPU_; }
	//vk::DeviceAddress getWaterRTVertexAddress() const { return waterRTVB_.getDeviceAddress(); }
	//vk::DeviceAddress getWaterRTIndexAddress() const { return waterRTIB_.getDeviceAddress(); }

	//const AccelerationStructureVk& getOpaqueBLAS() const { return opaqueBLAS_; }
	//const AccelerationStructureVk& getWaterBLAS() const { return waterBLAS_; }

private:
	void retireCurrentBuffers(uint32_t frameIndex);
	//void retireCurrentBLAS(uint32_t frameIndex);
private:
	DX12Main* dx_{ nullptr };

	//AccelerationStructureVk opaqueBLAS_;
	//AccelerationStructureVk waterBLAS_;

	//// RT opaque
	//BufferVk opaqueRTVB_;
	//BufferVk opaqueRTIB_;
	//uint32_t opaqueRTIndexCount_{ 0 };
	//uint32_t opaqueRTVertexCount_{ 0 };

	//std::vector<World::RTVertex> opaqueRTVerticesCPU_;
	//std::vector<uint32_t> opaqueRTIndicesCPU_;

	//// RT water
	//BufferVk waterRTVB_;
	//BufferVk waterRTIB_;
	//uint32_t waterRTIndexCount_{ 0 };
	//uint32_t waterRTVertexCount_{ 0 };

	//std::vector<World::RTVertex> waterRTVerticesCPU_;
	//std::vector<uint32_t> waterRTIndicesCPU_;

	// opaque
	BufferDX12 opaqueVB_;
	BufferDX12 opaqueIB_;
	uint32_t opaqueIndexCount_{ 0 };

	// water
	BufferDX12 waterVB_;
	BufferDX12 waterIB_;
	uint32_t waterIndexCount_{ 0 };
};

#endif
