#include "buffer_dx12.h"

#include "dx12_main.h"
#include "utils_dx12.h"

#include <stdexcept>
#include <cstring>

//--- HELPER ---//
static uint64_t AlignConstantBufferSize(uint64_t size)
{
	constexpr uint64_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	return (size + alignment - 1) & ~(alignment - 1);
} // end of AlignConstantBufferSize()


//--- PUBLIC ---//
BufferDX12::BufferDX12(DX12Main& dx)
	: dx_(&dx)
{
} // end of constructor

BufferDX12::~BufferDX12() = default;

void BufferDX12::create(
	uint64_t size,
	D3D12_HEAP_TYPE heapType,
	D3D12_RESOURCE_STATES initState,
	D3D12_RESOURCE_FLAGS flags,
	bool constantBuffer
)
{
	destroy();

	if (size == 0)
	{
		throw std::runtime_error("BufferDX12::create - size must be greater than 0");
	}

	if (constantBuffer)
	{
		size = AlignConstantBufferSize(size);
	}

	size_ = size;
	heapType_ = heapType;
	state_ = initState;

	D3D12_HEAP_PROPERTIES heapProps{
		.Type = heapType,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 1,
		.VisibleNodeMask = 1
	};

	D3D12_RESOURCE_DESC desc{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = 0,
		.Width = size_,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0
		},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = flags
	};

	DX12Utils::ThrowIfFailed(
		dx_->getDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			initState,
			nullptr,
			IID_PPV_ARGS(&buffer_)
		),
		"BufferDX12::create - failed to create DX12 buffer"
	);
} // end of create()

void BufferDX12::destroy()
{
	buffer_.Reset();

	size_ = 0;
	heapType_ = D3D12_HEAP_TYPE_DEFAULT;
	state_ = D3D12_RESOURCE_STATE_COMMON;
} // end of destroy

void BufferDX12::upload(
	const void* data,
	uint64_t size,
	uint64_t offset
)
{
	if (!buffer_)
	{
		throw std::runtime_error("BufferDX12::upload - invalid buffer");
	}

	if (!data)
	{
		throw std::runtime_error("BufferDX12::upload - data is null");
	}

	if (heapType_ != D3D12_HEAP_TYPE_UPLOAD)
	{
		throw std::runtime_error("BufferDX12::upload - buffer is not an UPLOAD heap buffer");
	}

	if (offset + size > size_)
	{
		throw std::runtime_error("BufferDX12::upload - write exceeds buffer size");
	}

	void* mappedData{ nullptr };

	D3D12_RANGE readRange{
		.Begin = 0,
		.End = 0
	};

	DX12Utils::ThrowIfFailed(
		buffer_->Map(0, &readRange, &mappedData),
		"BufferDX12::upload - failed to map DX12 upload buffer"
	);

	std::memcpy(
		static_cast<std::uint8_t*>(mappedData) + offset,
		data,
		static_cast<size_t>(size)
	);

	D3D12_RANGE writtenRange{
		.Begin = static_cast<SIZE_T>(offset),
		.End = static_cast<SIZE_T>(offset + size)
	};

	buffer_->Unmap(0, &writtenRange);
} // end of upload()

