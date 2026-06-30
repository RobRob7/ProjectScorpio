#ifndef DESCRIPTOR_SET_DX12_H
#define DESCRIPTOR_SET_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

class DX12Main;
class BufferDX12;
class ImageDX12;

using Microsoft::WRL::ComPtr;

enum class DescriptorTypeDX12
{
	UniformBuffer,
	StorageBufferSRV,
	StorageBufferUAV,
	TextureSRV,
	StorageImageUAV,
	AccelerationStructureSRV
};

struct DescriptorBindingDX12
{
	uint32_t binding{};
	DescriptorTypeDX12 type{};
	uint32_t count{ 1 };
	D3D12_SHADER_VISIBILITY visibility{ D3D12_SHADER_VISIBILITY_ALL };
};

struct PushConstantRangeDX12
{
	uint32_t binding{};
	uint32_t num32BitValues{};
	uint32_t registerSpace{ 1 };
	D3D12_SHADER_VISIBILITY visibility{ D3D12_SHADER_VISIBILITY_ALL };
};

class DescriptorSetDX12
{
public:
	explicit DescriptorSetDX12(DX12Main& dx);
	~DescriptorSetDX12();

	DescriptorSetDX12(const DescriptorSetDX12&) = delete;
	DescriptorSetDX12& operator=(const DescriptorSetDX12&) = delete;

	DescriptorSetDX12(DescriptorSetDX12&&) noexcept = default;
	DescriptorSetDX12& operator=(DescriptorSetDX12&&) noexcept = default;

	void setDebugName(const std::wstring& name);

	void createLayout(
		const std::vector<DescriptorBindingDX12>& bindings,
		const std::vector<PushConstantRangeDX12>& pushConstants = {}
	);

	void createPool(uint32_t descriptorCount);

	void allocate();

	void destroy();

	void writeUniformBuffer(
		uint32_t binding,
		const BufferDX12& buffer,
		uint64_t range,
		uint64_t offset = 0
	);

	void writeStorageBufferSRV(
		uint32_t binding,
		const BufferDX12& buffer,
		uint32_t numElements,
		uint32_t stride,
		uint64_t offset = 0
	);

	void writeStorageBufferUAV(
		uint32_t binding,
		const BufferDX12& buffer,
		uint32_t numElements,
		uint32_t stride,
		uint64_t offset = 0
	);

	void writeTextureSRV(
        uint32_t binding,
        const ImageDX12& image,
        D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN
    );

    void writeStorageImageUAV(
        uint32_t binding,
        const ImageDX12& image,
        D3D12_UAV_DIMENSION dimension = D3D12_UAV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN
    );

	void writeTextureSRVAtSlot(
		uint32_t slot,
		const ImageDX12& image,
		D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN,
		uint32_t mostDetailedMip = 0,
		uint32_t mipCount = UINT32_MAX
	);

	void writeStorageImageUAVAtSlot(
		uint32_t slot,
		const ImageDX12& image,
		D3D12_UAV_DIMENSION dimension = D3D12_UAV_DIMENSION_TEXTURE2D,
		DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN,
		uint32_t mipSlice = 0
	);

	D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t binding) const;
	D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t binding) const;
	D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandleAtSlot(uint32_t slot) const;
	D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandleAtSlot(uint32_t slot) const;

	D3D12_GPU_DESCRIPTOR_HANDLE getTableGPUHandle() const;

	bool valid() const 
	{ 
		return static_cast<bool>(rootSignature_) &&
			static_cast<bool>(descriptorHeap_);
	} // end of valid()

	ID3D12RootSignature* getRootSignature() const { return rootSignature_.Get(); }
	ID3D12DescriptorHeap* getDescriptorHeap() const { return descriptorHeap_.Get(); }
	uint32_t getDescriptorTableRootIndex() const { return descriptorTableRootIndex_; }


	template<typename T>
	void setGraphicsPushConstants(
		ID3D12GraphicsCommandList* cmd,
		uint32_t binding,
		const T& data
	) const
	{
		static_assert(sizeof(T) % 4 == 0, "Push constant data must be 4-byte aligned.");

		const uint32_t rootIndex = getPushConstantRootIndex(binding);

		cmd->SetGraphicsRoot32BitConstants(
			rootIndex,
			static_cast<UINT>(sizeof(T) / 4),
			&data,
			0
		);
	} // end of setGraphicsPushConstants()

private:
	uint32_t getSlot(uint32_t binding) const;
	uint32_t getPushConstantRootIndex(uint32_t binding) const;
private:
	DX12Main* dx_{ nullptr };

	std::wstring debugName_;

	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	UINT descriptorSize_{ 0 };
	uint32_t descriptorCount_{ 0 };

	std::vector<DescriptorBindingDX12> bindings_;
	std::unordered_map<uint32_t, uint32_t> bindingToSlot_;

	std::vector<PushConstantRangeDX12> pushConstants_;
	std::unordered_map<uint32_t, uint32_t> pushConstantBindingToRootIndex_;
	uint32_t descriptorTableRootIndex_{ 0 };
};

#endif
