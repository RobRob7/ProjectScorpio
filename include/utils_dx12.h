#ifndef UTILS_DX12_H
#define UTILS_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>

class DX12Main;

namespace DX12Utils 
{
	void ThrowIfFailed(HRESULT hr, const char* message);

	void TransitionResource(
		ID3D12GraphicsCommandList* cmd,
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES& oldState,
		D3D12_RESOURCE_STATES newState
	);

	void TransitionResourceImmediate(
		DX12Main& dx,
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES& oldState,
		D3D12_RESOURCE_STATES newState
	);

	void UAVBarrier(
		ID3D12GraphicsCommandList* cmd,
		ID3D12Resource* resource
	);

	void CopyBufferToTexture(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		ID3D12Resource* uploadBuffer,
		ID3D12Resource* texture,
		uint32_t layers,
		uint32_t mipLevels
	);

	void CopyBufferToTextureImmediate(
		DX12Main& dx,
		ID3D12Resource* uploadBuffer,
		ID3D12Resource* texture,
		uint32_t layers,
		uint32_t mipLevels
	);
}

#endif
