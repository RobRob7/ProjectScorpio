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
class ImageDX12;

namespace DX12Utils 
{
	void DumpInfoQueue(ID3D12Device* device);

	void ThrowIfFailed(
		ID3D12Device* device,
		HRESULT hr, 
		const char* message
	);

	void ThrowIfFailed(
		ID3D12Device* device,
		const char* message
	);

	void TransitionSubresource(
		ID3D12GraphicsCommandList* cmd,
		ID3D12Resource* resource,
		uint32_t mip,
		D3D12_RESOURCE_STATES& oldState,
		D3D12_RESOURCE_STATES newState
	);

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

	void UploadTexture2DArrayImmediate(
		DX12Main& dx,
		ImageDX12& image,
		const void* tightData,
		uint32_t width,
		uint32_t height,
		uint32_t layers,
		uint32_t bytesPerPixel,
		D3D12_RESOURCE_STATES finalState
	);
}

#endif
