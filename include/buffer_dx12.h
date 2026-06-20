#ifndef BUFFER_DX12_H
#define BUFFER_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

class DX12Main;

using Microsoft::WRL::ComPtr;

class BufferDX12
{
public:
	explicit BufferDX12(DX12Main& dx);
	~BufferDX12();

	BufferDX12(const BufferDX12&) = delete;
	BufferDX12& operator=(const BufferDX12&) = delete;

	BufferDX12(BufferDX12&&) noexcept = default;
	BufferDX12& operator=(BufferDX12&&) noexcept = default;

	void create(
		uint64_t size,
		D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_STATES initState,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
		bool constantBuffer = false
	);

	void destroy();

	void upload(
		const void* data,
		uint64_t size,
		uint64_t offset = 0
	);

	bool valid() const { return static_cast<bool>(buffer_); }
	
	ID3D12Resource* getResource() const { return buffer_.Get(); }

	D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() const { return buffer_ ? buffer_->GetGPUVirtualAddress() : 0; }

	uint64_t size() const { return size_; }

	D3D12_HEAP_TYPE heapType() const { return heapType_; }
	D3D12_RESOURCE_STATES state() const { return state_; }
	D3D12_RESOURCE_STATES& state() { return state_; }

private:
	DX12Main* dx_{ nullptr };

	ComPtr<ID3D12Resource> buffer_;

	uint64_t size_{ 0 };
	D3D12_HEAP_TYPE heapType_{ D3D12_HEAP_TYPE_DEFAULT };
	D3D12_RESOURCE_STATES state_{ D3D12_RESOURCE_STATE_COMMON };
};

#endif
