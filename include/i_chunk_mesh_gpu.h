#ifndef I_CHUNK_MESH_GPU_H
#define I_CHUNK_MESH_GPU_H

struct ChunkMeshData;
struct FrameContext;
struct FrameContextDX12;

class IChunkMeshGPU
{
public:
	virtual ~IChunkMeshGPU() = default;

	virtual void upload(
		const ChunkMeshData& data,
		const FrameContext* frameVk = nullptr,
		const FrameContextDX12* frameDX12 = nullptr
	) = 0;
	virtual void drawOpaque(
		const FrameContext* frameVk = nullptr,
		const FrameContextDX12* frameDX12 = nullptr
	) = 0;
	virtual void drawWater(
		const FrameContext* frameVk = nullptr,
		const FrameContextDX12* frameDX12 = nullptr
	) = 0;
};

#endif
