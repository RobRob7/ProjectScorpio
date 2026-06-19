#include "utils_dx12.h"

#include "dx12_main.h"

#include <stdexcept>
#include <vector>

namespace
{

}

namespace DX12Utils
{
	void ThrowIfFailed(HRESULT hr, const char* message)
	{
		if (FAILED(hr))
		{
			throw std::runtime_error(message);
		}
	} // end of ThrowIfFailed()

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
}