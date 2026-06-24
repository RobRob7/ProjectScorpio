#include "chunk_mesh_gpu_dx12.h"

#include "dx12_main.h"
#include "chunk_mesh_data.h"
#include "frame_context_dx12.h"
#include "utils_dx12.h"

#include <vector>
#include <utility>

using namespace World;

//--- HELPER ---//
static void UploadBufferDX12(
	DX12Main& dx,
	ID3D12GraphicsCommandList* cmd,
	const void* srcData,
	uint64_t size,
	D3D12_RESOURCE_STATES finalState,
	BufferDX12& dstBuffer,
	std::vector<BufferDX12>& stagingBuffers
)
{
	if (!srcData || size == 0)
	{
		return;
	}

	BufferDX12 staging(dx);

	staging.create(
		size,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE,
		false
	);

	staging.upload(srcData, size);

	dstBuffer.create(
		size,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_NONE,
		false
	);

	DX12Utils::TransitionResource(
		cmd,
		dstBuffer.getResource(),
		dstBuffer.state(),
		D3D12_RESOURCE_STATE_COPY_DEST
	);

	cmd->CopyBufferRegion(
		dstBuffer.getResource(),
		0,
		staging.getResource(),
		0,
		size
	);

	DX12Utils::TransitionResource(
		cmd,
		dstBuffer.getResource(),
		dstBuffer.state(),
		finalState
	);

	stagingBuffers.push_back(std::move(staging));
} // end of UploadBufferDX12()


//--- PUBLIC ---//
ChunkMeshGPUDX12::ChunkMeshGPUDX12(DX12Main& dx)
	: dx_(&dx),
	//opaqueBLAS_(vk),
	//opaqueRTVB_(vk),
	//opaqueRTIB_(vk),
	opaqueVB_(dx),
	opaqueIB_(dx),
	//waterBLAS_(vk),
	//waterRTVB_(vk),
	//waterRTIB_(vk),
	waterVB_(dx),
	waterIB_(dx)
{
} // end of constructor

ChunkMeshGPUDX12::~ChunkMeshGPUDX12()
{
	if (dx_)
	{
		const uint32_t frameIndex = dx_->currentFrameIndex();
		//retireCurrentBLAS(frameIndex);
		retireCurrentBuffers(frameIndex);
	}
} // end of destructor

void ChunkMeshGPUDX12::upload(
	const ChunkMeshData& data,
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12
)
{
	if (!frameDX12) return;

	ID3D12GraphicsCommandList* cmd = frameDX12->cmd;

	if (!cmd)
	{
		return;
	}
	
	const bool rtEnabled = dx_->supportsRayTracing();

	//BufferDX12 newOpaqueRTVB(*vk_);
	//BufferDX12 newOpaqueRTIB(*vk_);
	BufferDX12 newOpaqueVB(*dx_);
	BufferDX12 newOpaqueIB(*dx_);

	//BufferDX12 newWaterRTVB(*vk_);
	//BufferDX12 newWaterRTIB(*vk_);
	BufferDX12 newWaterVB(*dx_);
	BufferDX12 newWaterIB(*dx_);

	//uint32_t newOpaqueRTVertexCount = 0;
	//uint32_t newOpaqueRTIndexCount = 0;
	uint32_t newOpaqueIndexCount = 0;

	//uint32_t newWaterRTVertexCount = 0;
	//uint32_t newWaterRTIndexCount = 0;
	uint32_t newWaterIndexCount = 0;

	std::vector<BufferDX12> stagingBuffers;
	stagingBuffers.reserve(8);

	//// copy data to CPU side holders
	//if (rtEnabled)
	//{
	//	opaqueRTVerticesCPU_ = data.opaqueRTVertices;
	//	opaqueRTIndicesCPU_ = data.opaqueIndices;
	//	waterRTVerticesCPU_ = data.waterRTVertices;
	//	waterRTIndicesCPU_ = data.waterIndices;
	//}
	//else
	//{
	//	opaqueRTVerticesCPU_.clear();
	//	opaqueRTIndicesCPU_.clear();
	//	waterRTVerticesCPU_.clear();
	//	waterRTIndicesCPU_.clear();
	//}

	//// -------- RT OPAQUE --------
	//if (rtEnabled && !data.opaqueRTVertices.empty() && !data.opaqueIndices.empty())
	//{
	//	vk::DeviceSize vbSize = sizeof(RTVertex) * data.opaqueRTVertices.size();
	//	vk::DeviceSize ibSize = sizeof(uint32_t) * data.opaqueIndices.size();

	//	// RT VB staging
	//	BufferDX12 stagingVB(*vk_);
	//	stagingVB.create(
	//		vbSize,
	//		vk::BufferUsageFlagBits::eTransferSrc,
	//		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	//	);
	//	stagingVB.upload(data.opaqueRTVertices.data(), vbSize);

	//	// RT VB device local
	//	newOpaqueRTVB.create(
	//		vbSize,
	//		vk::BufferUsageFlagBits::eTransferDst |
	//		vk::BufferUsageFlagBits::eShaderDeviceAddress |
	//		vk::BufferUsageFlagBits::eStorageBuffer |
	//		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
	//		vk::MemoryPropertyFlagBits::eDeviceLocal,
	//		true
	//	);
	//	stagingBuffers.push_back(std::move(stagingVB));
	//	vk_->recordCopyBuffer(
	//		cmd,
	//		stagingBuffers.back().getBuffer(),
	//		newOpaqueRTVB.getBuffer(),
	//		vbSize
	//	);

	//	// RT IB staging
	//	BufferDX12 stagingIB(*vk_);
	//	stagingIB.create(
	//		ibSize,
	//		vk::BufferUsageFlagBits::eTransferSrc,
	//		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	//	);
	//	stagingIB.upload(data.opaqueIndices.data(), ibSize);

	//	// RT IB device local
	//	newOpaqueRTIB.create(
	//		ibSize,
	//		vk::BufferUsageFlagBits::eTransferDst |
	//		vk::BufferUsageFlagBits::eShaderDeviceAddress |
	//		vk::BufferUsageFlagBits::eStorageBuffer |
	//		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
	//		vk::MemoryPropertyFlagBits::eDeviceLocal,
	//		true
	//	);
	//	stagingBuffers.push_back(std::move(stagingIB));
	//	vk_->recordCopyBuffer(
	//		cmd,
	//		stagingBuffers.back().getBuffer(),
	//		newOpaqueRTIB.getBuffer(),
	//		ibSize
	//	);

	//	newOpaqueRTIndexCount = static_cast<uint32_t>(data.opaqueIndices.size());
	//	newOpaqueRTVertexCount = static_cast<uint32_t>(data.opaqueRTVertices.size());
	//}

	// -------- OPAQUE --------
	if (!data.opaqueVertices.empty() && !data.opaqueIndices.empty())
	{
		const uint64_t vbSize = 
			sizeof(Vertex) * data.opaqueVertices.size();
		const uint64_t ibSize = 
			sizeof(uint32_t) * data.opaqueIndices.size();

		// VB
		UploadBufferDX12(
			*dx_,
			cmd,
			data.opaqueVertices.data(),
			vbSize,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			newOpaqueVB,
			stagingBuffers
		);

		// IB
		UploadBufferDX12(
			*dx_,
			cmd,
			data.opaqueIndices.data(),
			ibSize,
			D3D12_RESOURCE_STATE_INDEX_BUFFER,
			newOpaqueIB,
			stagingBuffers
		);

		newOpaqueIndexCount = 
			static_cast<uint32_t>(data.opaqueIndices.size());
	}


	//// -------- RT WATER --------
	//if (rtEnabled && !data.waterRTVertices.empty() && !data.waterIndices.empty())
	//{
	//	vk::DeviceSize vbSize = sizeof(RTVertex) * data.waterRTVertices.size();
	//	vk::DeviceSize ibSize = sizeof(uint32_t) * data.waterIndices.size();

	//	// RT VB staging
	//	BufferDX12 stagingVB(*vk_);
	//	stagingVB.create(
	//		vbSize,
	//		vk::BufferUsageFlagBits::eTransferSrc,
	//		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	//	);
	//	stagingVB.upload(data.waterRTVertices.data(), vbSize);

	//	// RT VB device local
	//	newWaterRTVB.create(
	//		vbSize,
	//		vk::BufferUsageFlagBits::eTransferDst |
	//		vk::BufferUsageFlagBits::eShaderDeviceAddress |
	//		vk::BufferUsageFlagBits::eStorageBuffer |
	//		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
	//		vk::MemoryPropertyFlagBits::eDeviceLocal,
	//		true
	//	);
	//	stagingBuffers.push_back(std::move(stagingVB));
	//	vk_->recordCopyBuffer(
	//		cmd,
	//		stagingBuffers.back().getBuffer(),
	//		newWaterRTVB.getBuffer(),
	//		vbSize
	//	);

	//	// RT IB staging
	//	BufferDX12 stagingIB(*vk_);
	//	stagingIB.create(
	//		ibSize,
	//		vk::BufferUsageFlagBits::eTransferSrc,
	//		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	//	);
	//	stagingIB.upload(data.waterIndices.data(), ibSize);

	//	// RT IB device local
	//	newWaterRTIB.create(
	//		ibSize,
	//		vk::BufferUsageFlagBits::eTransferDst |
	//		vk::BufferUsageFlagBits::eShaderDeviceAddress |
	//		vk::BufferUsageFlagBits::eStorageBuffer |
	//		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
	//		vk::MemoryPropertyFlagBits::eDeviceLocal,
	//		true
	//	);
	//	stagingBuffers.push_back(std::move(stagingIB));
	//	vk_->recordCopyBuffer(
	//		cmd,
	//		stagingBuffers.back().getBuffer(),
	//		newWaterRTIB.getBuffer(),
	//		ibSize
	//	);

	//	newWaterRTIndexCount = static_cast<uint32_t>(data.waterIndices.size());
	//	newWaterRTVertexCount = static_cast<uint32_t>(data.waterRTVertices.size());
	//}

	// -------- WATER --------
	if (!data.waterVertices.empty() && !data.waterIndices.empty())
	{
		const uint64_t vbSize = 
			sizeof(VertexWater) * data.waterVertices.size();
		const uint64_t ibSize = 
			sizeof(uint32_t) * data.waterIndices.size();

		// VB 
		UploadBufferDX12(
			*dx_,
			cmd,
			data.waterVertices.data(),
			vbSize,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			newWaterVB,
			stagingBuffers
		);

		// IB
		UploadBufferDX12(
			*dx_,
			cmd,
			data.waterIndices.data(),
			ibSize,
			D3D12_RESOURCE_STATE_INDEX_BUFFER,
			newWaterIB,
			stagingBuffers
		);

		newWaterIndexCount = 
			static_cast<uint32_t>(data.waterIndices.size());
	}

	const uint32_t frameIndex = dx_->currentFrameIndex();
	if (rtEnabled)
	{
		//retireCurrentBLAS(frameIndex);
	}
	retireCurrentBuffers(frameIndex);

	//opaqueRTVB_ = std::move(newOpaqueRTVB);
	//opaqueRTIB_ = std::move(newOpaqueRTIB);
	opaqueVB_ = std::move(newOpaqueVB);
	opaqueIB_ = std::move(newOpaqueIB);

	//waterRTVB_ = std::move(newWaterRTVB);
	//waterRTIB_ = std::move(newWaterRTIB);
	waterVB_ = std::move(newWaterVB);
	waterIB_ = std::move(newWaterIB);

	//opaqueRTVertexCount_ = newOpaqueRTVertexCount;
	//opaqueRTIndexCount_ = newOpaqueRTIndexCount;
	opaqueIndexCount_ = newOpaqueIndexCount;

	//waterRTVertexCount_ = newWaterRTVertexCount;
	//waterRTIndexCount_ = newWaterRTIndexCount;
	waterIndexCount_ = newWaterIndexCount;

	//if (rtEnabled)
	//{
	//	std::vector<vk::BufferMemoryBarrier> barriers;

	//	auto addBarrier = [&](const BufferDX12& buffer)
	//		{
	//			if (!buffer.valid())
	//				return;

	//			vk::BufferMemoryBarrier b{};
	//			b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	//			b.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR;
	//			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//			b.buffer = buffer.getBuffer();
	//			b.offset = 0;
	//			b.size = VK_WHOLE_SIZE;

	//			barriers.push_back(b);
	//		};

	//	addBarrier(opaqueRTVB_);
	//	addBarrier(opaqueRTIB_);
	//	addBarrier(waterRTVB_);
	//	addBarrier(waterRTIB_);

	//	if (!barriers.empty())
	//	{
	//		cmd.pipelineBarrier(
	//			vk::PipelineStageFlagBits::eTransfer,
	//			vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
	//			{},
	//			0, nullptr,
	//			static_cast<uint32_t>(barriers.size()),
	//			barriers.data(),
	//			0, nullptr
	//		);
	//	}

	//	// build opaque BLAS
	//	if (opaqueRTVB_.valid() && opaqueRTIB_.valid() &&
	//		opaqueRTVertexCount_ > 0 && opaqueRTIndexCount_ > 0)
	//	{
	//		opaqueBLAS_.buildBLASOnCmd(
	//			cmd,
	//			opaqueRTVB_.getBuffer(),
	//			opaqueRTVB_.getDeviceAddress(),
	//			opaqueRTVertexCount_,
	//			sizeof(RTVertex),
	//			opaqueRTIB_.getBuffer(),
	//			opaqueRTIB_.getDeviceAddress(),
	//			opaqueRTIndexCount_,
	//			vk::IndexType::eUint32
	//		);
	//	}
	//	// build water BLAS
	//	if (waterRTVB_.valid() && waterRTIB_.valid() &&
	//		waterRTVertexCount_ > 0 && waterRTIndexCount_ > 0)
	//	{
	//		waterBLAS_.buildBLASOnCmd(
	//			cmd,
	//			waterRTVB_.getBuffer(),
	//			waterRTVB_.getDeviceAddress(),
	//			waterRTVertexCount_,
	//			sizeof(RTVertex),
	//			waterRTIB_.getBuffer(),
	//			waterRTIB_.getDeviceAddress(),
	//			waterRTIndexCount_,
	//			vk::IndexType::eUint32
	//		);
	//	}
	//}

	for (auto& staging : stagingBuffers)
	{
		dx_->retireBuffer(frameIndex, std::move(staging));
	} // end for

	stagingBuffers.clear();
} // end of upload()

void ChunkMeshGPUDX12::drawOpaque(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12
)
{
	if (!frameDX12) return;

	ID3D12GraphicsCommandList* cmd = frameDX12->cmd;

	if (!cmd || opaqueIndexCount_ == 0 || !opaqueVB_.valid() || !opaqueIB_.valid())
	{
		return;
	}

	D3D12_VERTEX_BUFFER_VIEW vbView{
		.BufferLocation = opaqueVB_.getGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(opaqueVB_.size()),
		.StrideInBytes = sizeof(Vertex)
	};

	D3D12_INDEX_BUFFER_VIEW ibView{
		.BufferLocation = opaqueIB_.getGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(opaqueIB_.size()),
		.Format = DXGI_FORMAT_R32_UINT
	};

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetIndexBuffer(&ibView);

	cmd->DrawIndexedInstanced(
		opaqueIndexCount_,
		1,
		0,
		0,
		0
	);
} // end of drawOpaque()

void ChunkMeshGPUDX12::drawWater(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12
)
{
	if (!frameDX12) return;

	ID3D12GraphicsCommandList* cmd = frameDX12->cmd;

	if (!cmd || waterIndexCount_ == 0 || !waterVB_.valid() || !waterIB_.valid())
	{
		return;
	}
	
	D3D12_VERTEX_BUFFER_VIEW vbView{
	.BufferLocation = waterVB_.getGPUVirtualAddress(),
	.SizeInBytes = static_cast<UINT>(waterVB_.size()),
	.StrideInBytes = sizeof(VertexWater)
	};

	D3D12_INDEX_BUFFER_VIEW ibView{
		.BufferLocation = waterIB_.getGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(waterIB_.size()),
		.Format = DXGI_FORMAT_R32_UINT
	};

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetIndexBuffer(&ibView);

	cmd->DrawIndexedInstanced(
		waterIndexCount_,
		1,
		0,
		0,
		0
	);
} // end of drawWater()


//--- PRIVATE ---//
void ChunkMeshGPUDX12::retireCurrentBuffers(uint32_t frameIndex)
{
	//if (!opaqueRTVB_.valid() && !opaqueRTIB_.valid() &&
	//	!opaqueVB_.valid() && !opaqueIB_.valid() &&
	//	!waterRTVB_.valid() && !waterRTIB_.valid() &&
	//	!waterVB_.valid() && !waterIB_.valid())
	//{
	//	return;
	//}

	//vk_->retireBuffer(frameIndex, std::move(opaqueRTVB_));
	//vk_->retireBuffer(frameIndex, std::move(opaqueRTIB_));
	dx_->retireBuffer(frameIndex, std::move(opaqueVB_));
	dx_->retireBuffer(frameIndex, std::move(opaqueIB_));

	//vk_->retireBuffer(frameIndex, std::move(waterRTVB_));
	//vk_->retireBuffer(frameIndex, std::move(waterRTIB_));
	dx_->retireBuffer(frameIndex, std::move(waterVB_));
	dx_->retireBuffer(frameIndex, std::move(waterIB_));
} // end of retireCurrentBuffers()

//void ChunkMeshGPUDX12::retireCurrentBLAS(uint32_t frameIndex)
//{
//	if (opaqueBLAS_.valid())
//	{
//		vk_->retireAccelerationStructure(frameIndex, std::move(opaqueBLAS_));
//		opaqueBLAS_ = AccelerationStructureVk(*vk_);
//	}
//	if (waterBLAS_.valid())
//	{
//		vk_->retireAccelerationStructure(frameIndex, std::move(waterBLAS_));
//		waterBLAS_ = AccelerationStructureVk(*vk_);
//	}
//} // end of retireCurrentBLAS()
