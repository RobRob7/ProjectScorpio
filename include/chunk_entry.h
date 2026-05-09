#ifndef CHUNK_ENTRY_H
#define CHUNK_ENTRY_H

#include "chunk_mesh.h"

#include "chunk_mesh_gpu_gl.h"
#include "chunk_mesh_gpu_vk.h"

class IChunkMeshGPU;
class VulkanMain;

#include <memory>
#include <cstdint>

struct ChunkEntry
{
	std::unique_ptr<ChunkMesh> cpu;
	std::shared_ptr<IChunkMeshGPU> gpu;

	uint64_t geometryVersion = 0;

	ChunkEntry(int chunkX, int chunkZ, VulkanMain* vk)
	{
		cpu = std::make_unique<ChunkMesh>(chunkX, chunkZ, false);

		if (vk)
		{
			gpu = std::make_shared<ChunkMeshGPUVk>(*vk);
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

	void uploadGPU()
	{
		gpu->upload(cpu->data());
		++geometryVersion;
	} // end of uploadGPU()
};

#endif
