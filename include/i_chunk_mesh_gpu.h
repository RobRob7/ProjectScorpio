#ifndef I_CHUNK_MESH_GPU_H
#define I_CHUNK_MESH_GPU_H

#include <vulkan/vulkan.hpp>

struct ChunkMeshData;

class IChunkMeshGPU
{
public:
	virtual ~IChunkMeshGPU() = default;

	virtual void upload(
		vk::CommandBuffer cmd, 
		const ChunkMeshData& data
	) = 0;
	virtual void drawOpaque(vk::CommandBuffer cmd) = 0;
	virtual void drawWater(vk::CommandBuffer cmd) = 0;
};

#endif
