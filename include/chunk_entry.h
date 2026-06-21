#ifndef CHUNK_ENTRY_H
#define CHUNK_ENTRY_H

#include "chunk_mesh.h"

#include "chunk_mesh_gpu_gl.h"
#include "chunk_mesh_gpu_vk.h"
#include "chunk_mesh_gpu_dx12.h"

#include <memory>
#include <cstdint>
#include <stdexcept>

class IChunkMeshGPU;
class VulkanMain;
class DX12Main;
class ContextFrame;
class ContextFrameDX12;

struct ChunkEntry
{
	std::unique_ptr<ChunkMesh> cpu;
	std::shared_ptr<IChunkMeshGPU> gpu;

	uint64_t geometryVersion = 0;

	ChunkEntry(
		int chunkX, 
		int chunkZ, 
		VulkanMain* vk,
		DX12Main* dx
	)
	{
		if (vk && dx)
		{
			throw std::runtime_error("ChunkEntry::constructor - can only have ONE API set");
		}

		cpu = std::make_unique<ChunkMesh>(chunkX, chunkZ, false);

		if (vk)
		{
			gpu = std::make_shared<ChunkMeshGPUVk>(*vk);
		}
		else if (dx)
		{
			gpu = std::make_shared<ChunkMeshGPUDX12>(*dx);
		}
		else
		{
			gpu = std::make_shared<ChunkMeshGPUGL>();
		}
	} // end of constructor

	void rebuildCPU() const
	{
		cpu->rebuild();
	} // end of rebuildCPU()

	void uploadGPU(
		const FrameContext* frameVk = nullptr,
		const FrameContextDX12* frameDX12 = nullptr
	)
	{
		gpu->upload(cpu->data(), frameVk, frameDX12);
		++geometryVersion;
	} // end of uploadGPU()
};

#endif
