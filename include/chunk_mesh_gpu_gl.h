#ifndef CHUNK_MESH_GPU_GL_H
#define CHUNK_MESH_GPU_GL_H

#include "i_chunk_mesh_gpu.h"

#include <cstdint>

class ChunkMeshGPUGL final : public IChunkMeshGPU
{
public:
    ChunkMeshGPUGL();
	~ChunkMeshGPUGL() override;

	void upload(
        vk::CommandBuffer,
        const ChunkMeshData& data
    ) override;
	void drawOpaque(vk::CommandBuffer cmd) override;
	void drawWater(vk::CommandBuffer cmd) override;

private:
    // opaque
    uint32_t opaqueVao_{};
    uint32_t opaqueVbo_{};
    uint32_t opaqueEbo_{};
    int32_t opaqueIndexCount_{};

	// water
    uint32_t waterVao_{};
    uint32_t waterVbo_{};
    uint32_t waterEbo_{};
    int32_t waterIndexCount_{};
};

#endif
