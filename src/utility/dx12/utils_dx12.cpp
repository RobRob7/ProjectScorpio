#include "utils_dx12.h"

#include "dx12_main.h"
#include "image_dx12.h"

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iostream>

namespace
{
	void ExecuteAndWait(
		DX12Main& dx,
		ID3D12GraphicsCommandList* cmd
	)
	{
		DX12Utils::ThrowIfFailed(
			dx.getDevice(),
			cmd->Close(),
			"DX12Utils::ExecuteAndWait - failed to close command list"
		);

		ID3D12CommandList* commandLists[] =
		{
			cmd
		};

		dx.getGraphicsQueue()->ExecuteCommandLists(1, commandLists);

		ComPtr<ID3D12Fence> fence;

		DX12Utils::ThrowIfFailed(
			dx.getDevice(),
			dx.getDevice()->CreateFence(
				0,
				D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(&fence)
			),
			"DX12Utils::ExecuteAndWait - failed to create fence"
		);

		HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		if (!fenceEvent)
		{
			throw std::runtime_error("DX12Utils::ExecuteAndWait - failed to create fence event");
		}

		const uint64_t fenceValue = 1;

		DX12Utils::ThrowIfFailed(
			dx.getDevice(),
			dx.getGraphicsQueue()->Signal(fence.Get(), fenceValue),
			"DX12Utils::ExecuteAndWait - failed to signal fence"
		);

		if (fence->GetCompletedValue() < fenceValue)
		{
			DX12Utils::ThrowIfFailed(
				dx.getDevice(),
				fence->SetEventOnCompletion(fenceValue, fenceEvent),
				"DX12Utils::ExecuteAndWait - failed to set fence event"
			);

			WaitForSingleObject(fenceEvent, INFINITE);
		}

		CloseHandle(fenceEvent);
	} // end of ExecuteAndWait()
}

namespace DX12Utils
{
	void DumpInfoQueue(ID3D12Device* device)
	{
#if defined(_DEBUG)
		if (!device)
		{
			return;
		}

		ComPtr<ID3D12InfoQueue> infoQueue;

		if (FAILED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			return;
		}

		const UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

		for (UINT64 i = 0; i < messageCount; ++i)
		{
			SIZE_T messageLength = 0;

			infoQueue->GetMessage(i, nullptr, &messageLength);

			std::vector<char> bytes(messageLength);
			auto* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());

			if (SUCCEEDED(infoQueue->GetMessage(i, message, &messageLength)))
			{
				std::ostringstream oss;
				oss << "[D3D12 InfoQueue] "
					<< "Severity=" << message->Severity
					<< " Category=" << message->Category
					<< " ID=" << message->ID
					<< "\n"
					<< message->pDescription
					<< "\n\n";

				OutputDebugStringA(oss.str().c_str());
				std::cerr << oss.str();
			}
		}

		infoQueue->ClearStoredMessages();
#endif
	} // end of DumpInfoQueue()

	void ThrowIfFailed(
		ID3D12Device* device, 
		HRESULT hr, 
		const char* message
	)
	{
		if (FAILED(hr))
		{
			DumpInfoQueue(device);

			char buffer[512];
			sprintf_s(
				buffer,
				"%s HRESULT=0x%08X",
				message,
				static_cast<unsigned>(hr)
			);
		
			throw std::runtime_error(buffer);
		}
	} // end of ThrowIfFailed()

	void ThrowIfFailed(
		ID3D12Device* device, 
		const char* message
	)
	{
		DumpInfoQueue(device);
		
		throw std::runtime_error(message);
	} // end of ThrowIfFailed()

	void TransitionSubresource(
		ID3D12GraphicsCommandList* cmd,
		ID3D12Resource* resource,
		uint32_t mip,
		D3D12_RESOURCE_STATES& oldState,
		D3D12_RESOURCE_STATES newState
	)
	{
		if (!cmd)
		{
			return;
		}

		if (oldState == newState)
		{
			return;
		}

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = mip;
		barrier.Transition.StateBefore = oldState;
		barrier.Transition.StateAfter = newState;

		cmd->ResourceBarrier(1, &barrier);

		oldState = newState;
	} // end of TransitionSubresource()

	void TransitionResource(
		ID3D12GraphicsCommandList* cmd,
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES& oldState,
		D3D12_RESOURCE_STATES newState
	)
	{
		if (!cmd || !resource)
		{
			return;
		}

		if (oldState == newState)
		{
			return;
		}

		D3D12_RESOURCE_BARRIER barrier{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition = {
				.pResource = resource,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = oldState,
				.StateAfter = newState
			}
		};

		cmd->ResourceBarrier(1, &barrier);

		oldState = newState;
	} // end of TransitionResource()

	void TransitionResourceImmediate(
		DX12Main& dx,
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES& oldState,
		D3D12_RESOURCE_STATES newState
	)
	{
		if (!resource || oldState == newState)
		{
			return;
		}

		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList4> cmd;

		ThrowIfFailed(
			dx.getDevice(),
			dx.getDevice()->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&allocator)
			),
			"DX12Utils::TransitionResourceImmediate - failed to create command allocator"
		);

		ThrowIfFailed(
			dx.getDevice(),
			dx.getDevice()->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				allocator.Get(),
				nullptr,
				IID_PPV_ARGS(&cmd)
			),
			"DX12Utils::TransitionResourceImmediate - failed to create command list"
		);

		TransitionResource(
			cmd.Get(),
			resource,
			oldState,
			newState
		);

		ExecuteAndWait(dx, cmd.Get());
	} // end of TransitionResourceImmediate()

	void CopyBufferToTexture(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		ID3D12Resource* uploadBuffer,
		ID3D12Resource* texture,
		uint32_t layers,
		uint32_t mipLevels
	)
	{
		if (!device || !cmd || !uploadBuffer || !texture)
		{
			return;
		}

		D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();

		const uint32_t subresourceCount = layers * mipLevels;

		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresourceCount);
		std::vector<UINT> numRows(subresourceCount);
		std::vector<UINT64> rowSizes(subresourceCount);

		UINT64 totalBytes = 0;

		device->GetCopyableFootprints(
			&textureDesc,
			0,
			subresourceCount,
			0,
			layouts.data(),
			numRows.data(),
			rowSizes.data(),
			&totalBytes
		);

		for (uint32_t subresource = 0; subresource < subresourceCount; ++subresource)
		{
			D3D12_TEXTURE_COPY_LOCATION src{};
			src.pResource = uploadBuffer;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = layouts[subresource];

			D3D12_TEXTURE_COPY_LOCATION dst{};
			dst.pResource = texture;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = subresource;

			cmd->CopyTextureRegion(
				&dst,
				0,
				0,
				0,
				&src,
				nullptr
			);
		} // end for
	} // end of CopyBufferToTexture()

	void CopyBufferToTextureImmediate(
		DX12Main& dx,
		ID3D12Resource* uploadBuffer,
		ID3D12Resource* texture,
		uint32_t layers,
		uint32_t mipLevels
	)
	{
		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList4> cmd;

		ThrowIfFailed(
			dx.getDevice(),
			dx.getDevice()->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&allocator)
			),
			"DX12Utils::CopyBufferToTextureImmediate - failed to create command allocator"
		);

		ThrowIfFailed(
			dx.getDevice(),
			dx.getDevice()->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				allocator.Get(),
				nullptr,
				IID_PPV_ARGS(&cmd)
			),
			"DX12Utils::CopyBufferToTextureImmediate - failed to create command list"
		);

		CopyBufferToTexture(
			dx.getDevice(),
			cmd.Get(),
			uploadBuffer,
			texture,
			layers,
			mipLevels
		);

		ExecuteAndWait(dx, cmd.Get());
	} // end of CopyBufferToTextureImmediate()

	void UploadTexture2DArrayImmediate(
		DX12Main& dx,
		ImageDX12& image,
		const void* tightData,
		uint32_t width,
		uint32_t height,
		uint32_t layers,
		uint32_t bytesPerPixel,
		D3D12_RESOURCE_STATES finalState
	)
	{
		if (!image.valid())
		{
			throw std::runtime_error("DX12Utils::UploadTexture2DArrayImmediate - invalid image");
		}

		if (!tightData)
		{
			throw std::runtime_error("DX12Utils::UploadTexture2DArrayImmediate - data is null");
		}

		if (width == 0 || height == 0 || layers == 0 || bytesPerPixel == 0)
		{
			throw std::runtime_error("DX12Utils::UploadTexture2DArrayImmediate - invalid dimensions");
		}

		ID3D12Resource* texture = image.resource();

		D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();

		const uint32_t subresourceCount = layers;

		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresourceCount);
		std::vector<UINT> numRows(subresourceCount);
		std::vector<UINT64> rowSizesInBytes(subresourceCount);

		UINT64 uploadBufferSize = 0;

		dx.getDevice()->GetCopyableFootprints(
			&textureDesc,
			0,
			subresourceCount,
			0,
			layouts.data(),
			numRows.data(),
			rowSizesInBytes.data(),
			&uploadBufferSize
		);

		BufferDX12 uploadBuffer(dx);

		uploadBuffer.create(
			uploadBufferSize,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			false
		);

		void* mappedData = nullptr;

		D3D12_RANGE readRange{};
		readRange.Begin = 0;
		readRange.End = 0;

		DX12Utils::ThrowIfFailed(
			dx.getDevice(),
			uploadBuffer.getResource()->Map(0, &readRange, &mappedData),
			"DX12Utils::UploadTexture2DArrayImmediate - failed to map upload buffer"
		);

		const uint64_t tightRowPitch =
			static_cast<uint64_t>(width) * bytesPerPixel;

		const uint64_t tightLayerSize =
			tightRowPitch * height;

		const auto* srcBytes =
			static_cast<const std::uint8_t*>(tightData);

		for (uint32_t layer = 0; layer < layers; ++layer)
		{
			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[layer];

			auto* dstLayer =
				static_cast<std::uint8_t*>(mappedData) + layout.Offset;

			const auto* srcLayer =
				srcBytes + static_cast<uint64_t>(layer) * tightLayerSize;

			for (uint32_t row = 0; row < height; ++row)
			{
				std::memcpy(
					dstLayer + static_cast<uint64_t>(row) * layout.Footprint.RowPitch,
					srcLayer + static_cast<uint64_t>(row) * tightRowPitch,
					static_cast<size_t>(tightRowPitch)
				);
			}
		}

		D3D12_RANGE writtenRange{};
		writtenRange.Begin = 0;
		writtenRange.End = static_cast<SIZE_T>(uploadBufferSize);

		uploadBuffer.getResource()->Unmap(0, &writtenRange);

		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList4> cmd;

		DX12Utils::ThrowIfFailed(
			dx.getDevice(),
			dx.getDevice()->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&allocator)
			),
			"DX12Utils::UploadTexture2DArrayImmediate - failed to create command allocator"
		);

		DX12Utils::ThrowIfFailed(
			dx.getDevice(),
			dx.getDevice()->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				allocator.Get(),
				nullptr,
				IID_PPV_ARGS(&cmd)
			),
			"DX12Utils::UploadTexture2DArrayImmediate - failed to create command list"
		);

		DX12Utils::TransitionResource(
			cmd.Get(),
			texture,
			image.state(),
			D3D12_RESOURCE_STATE_COPY_DEST
		);

		for (uint32_t layer = 0; layer < layers; ++layer)
		{
			D3D12_TEXTURE_COPY_LOCATION src{};
			src.pResource = uploadBuffer.getResource();
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = layouts[layer];

			D3D12_TEXTURE_COPY_LOCATION dst{};
			dst.pResource = texture;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = layer;

			cmd->CopyTextureRegion(
				&dst,
				0,
				0,
				0,
				&src,
				nullptr
			);
		}

		DX12Utils::TransitionResource(
			cmd.Get(),
			texture,
			image.state(),
			finalState
		);

		ExecuteAndWait(dx, cmd.Get());
	} // end of UploadTexture2DArrayImmediate()
}