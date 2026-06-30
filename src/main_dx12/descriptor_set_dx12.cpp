#include "descriptor_set_dx12.h"

#include "dx12_main.h"
#include "buffer_dx12.h"
#include "image_dx12.h"
#include "utils_dx12.h"

#include <stdexcept>
#include <cstdint>
#include <algorithm>

//--- HELPER ---//
static uint64_t AlignConstantBufferSize(uint64_t size)
{
	constexpr uint64_t alignment =
		D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

	return (size + alignment - 1) & ~(alignment - 1);
} // end of AlignConstantBufferSize()


//--- PUBLIC ---//
DescriptorSetDX12::DescriptorSetDX12(DX12Main& dx)
	: dx_(&dx)
{
} // end of constructor

DescriptorSetDX12::~DescriptorSetDX12() = default;

void DescriptorSetDX12::setDebugName(const std::wstring& name)
{
	debugName_ = name;

	if (rootSignature_)
	{
		rootSignature_->SetName(name.c_str());
	}
} // end of setDebugName()

void DescriptorSetDX12::createLayout(
	const std::vector<DescriptorBindingDX12>& bindings,
	const std::vector<PushConstantRangeDX12>& pushConstants
)
{
	if (bindings.empty() && pushConstants.empty())
	{
		throw std::runtime_error("DescriptorSetDX12::createLayout - layout cannot be empty");
	}

	bindings_ = bindings;
	pushConstants_ = pushConstants;
	pushConstantBindingToRootIndex_.clear();

	std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
	ranges.reserve(bindings_.size());

	for (const auto& binding : bindings_)
	{
		D3D12_DESCRIPTOR_RANGE range{};
		range.NumDescriptors = binding.count;
		range.BaseShaderRegister = binding.binding;
		range.RegisterSpace = 0;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		switch (binding.type)
		{
		case DescriptorTypeDX12::UniformBuffer:
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			break;

		case DescriptorTypeDX12::StorageBufferSRV:
		case DescriptorTypeDX12::TextureSRV:
		case DescriptorTypeDX12::AccelerationStructureSRV:
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			break;

		case DescriptorTypeDX12::StorageBufferUAV:
		case DescriptorTypeDX12::StorageImageUAV:
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			break;
		}

		ranges.push_back(range);
	} // end for

	std::vector<D3D12_ROOT_PARAMETER> rootParams;

	if (!ranges.empty())
	{
		descriptorTableRootIndex_ = static_cast<uint32_t>(rootParams.size());

		D3D12_ROOT_PARAMETER tableParam{};
		tableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		tableParam.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
		tableParam.DescriptorTable.pDescriptorRanges = ranges.data();
		tableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams.push_back(tableParam);
	}

	for (const auto& pc : pushConstants_)
	{
		const uint32_t rootIndex = static_cast<uint32_t>(rootParams.size());
		pushConstantBindingToRootIndex_[pc.binding] = rootIndex;

		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param.ShaderVisibility = pc.visibility;
		param.Constants.ShaderRegister = pc.binding;
		param.Constants.RegisterSpace = pc.registerSpace;
		param.Constants.Num32BitValues = pc.num32BitValues;

		rootParams.push_back(param);
	} // end for

	D3D12_STATIC_SAMPLER_DESC staticSampler{
		.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.MipLODBias = 0.0f,
		.MaxAnisotropy = 1,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
		.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
		.MinLOD = 0.0f,
		.MaxLOD = 7.0f,
		.ShaderRegister = 0, // register(s0)
		.RegisterSpace = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	};
	D3D12_STATIC_SAMPLER_DESC staticSampler1{
		.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.MipLODBias = 0.0f,
		.MaxAnisotropy = 1,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
		.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
		.MinLOD = 0.0f,
		.MaxLOD = 7.0f,
		.ShaderRegister = 1, // register(s1)
		.RegisterSpace = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	};

	D3D12_STATIC_SAMPLER_DESC staticSamplers[] =
	{
		staticSampler,
		staticSampler1
	};
	
	D3D12_ROOT_SIGNATURE_DESC rootDesc{
		.NumParameters = static_cast<UINT>(rootParams.size()),
		.pParameters = rootParams.data(),
		.NumStaticSamplers = _countof(staticSamplers),
		.pStaticSamplers = staticSamplers,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	};

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signature,
		&error
	);

	if (FAILED(hr))
	{
		throw std::runtime_error("DescriptorSetDX12::createLayout - failed to serialize root signature");
	}

	DX12Utils::ThrowIfFailed(
		dx_->getDevice(),
		dx_->getDevice()->CreateRootSignature(
			0,
			signature->GetBufferPointer(),
			signature->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature_)
		),
		"DescriptorSetDX12::createLayout - failed to create root signature"
	);

	if (!debugName_.empty())
	{
		rootSignature_->SetName(debugName_.c_str());
	}
} // end of createLayout()

void DescriptorSetDX12::createPool(uint32_t descriptorCount)
{
	descriptorCount_ = descriptorCount;

	D3D12_DESCRIPTOR_HEAP_DESC desc{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = descriptorCount_,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		.NodeMask = 0
	};

	DX12Utils::ThrowIfFailed(
		dx_->getDevice(),
		dx_->getDevice()->CreateDescriptorHeap(
			&desc,
			IID_PPV_ARGS(&descriptorHeap_)
		),
		"DescriptorSetDX12::createPool - failed to create descriptor heap"
	);

	descriptorSize_ =
		dx_->getDevice()->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
} // end of createPool()

void DescriptorSetDX12::allocate()
{
	if (!descriptorHeap_)
	{
		throw std::runtime_error("DescriptorSetDX12::allocate - descriptor heap not created");
	}

	bindingToSlot_.clear();

	uint32_t slot = 0;

	for (const auto& binding : bindings_)
	{
		bindingToSlot_[binding.binding] = slot;
		slot += binding.count;
	}

	if (slot > descriptorCount_)
	{
		throw std::runtime_error("DescriptorSetDX12::allocate - not enough descriptors in heap");
	}
} // end of allocate()

void DescriptorSetDX12::destroy()
{
	rootSignature_.Reset();
	descriptorHeap_.Reset();

	descriptorSize_ = 0;
	descriptorCount_ = 0;

	bindings_.clear();
	bindingToSlot_.clear();

	pushConstants_.clear();
	pushConstantBindingToRootIndex_.clear();
	descriptorTableRootIndex_ = 0;
} // end of destroy()

void DescriptorSetDX12::writeUniformBuffer(
	uint32_t binding,
	const BufferDX12& buffer,
	uint64_t range,
	uint64_t offset
)
{
	if (!buffer.valid())
	{
		throw std::runtime_error("DescriptorSetDX12::writeUniformBuffer - invalid buffer");
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
	desc.BufferLocation = buffer.getGPUVirtualAddress() + offset;
	desc.SizeInBytes = static_cast<UINT>(AlignConstantBufferSize(range));

	dx_->getDevice()->CreateConstantBufferView(
		&desc,
		getCPUHandle(binding)
	);
} // end of writeUniformBuffer()

void DescriptorSetDX12::writeStorageBufferSRV(
	uint32_t binding,
	const BufferDX12& buffer,
	uint32_t numElements,
	uint32_t stride,
	uint64_t offset
)
{
	if (!buffer.valid())
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageBufferSRV - invalid buffer");
	}

	if (stride == 0)
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageBufferSRV - stride cannot be 0");
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Buffer.FirstElement = offset / stride;
	desc.Buffer.NumElements = numElements;
	desc.Buffer.StructureByteStride = stride;
	desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	dx_->getDevice()->CreateShaderResourceView(
		buffer.getResource(),
		&desc,
		getCPUHandle(binding)
	);
} // end of writeStorageBufferSRV()

void DescriptorSetDX12::writeStorageBufferUAV(
	uint32_t binding,
	const BufferDX12& buffer,
	uint32_t numElements,
	uint32_t stride,
	uint64_t offset
)
{
	if (!buffer.valid())
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageBufferUAV - invalid buffer");
	}

	if (stride == 0)
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageBufferUAV - stride cannot be 0");
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = offset / stride;
	desc.Buffer.NumElements = numElements;
	desc.Buffer.StructureByteStride = stride;
	desc.Buffer.CounterOffsetInBytes = 0;
	desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dx_->getDevice()->CreateUnorderedAccessView(
		buffer.getResource(),
		nullptr,
		&desc,
		getCPUHandle(binding)
	);
} // end of writeStorageBufferUAV()

void DescriptorSetDX12::writeTextureSRV(
	uint32_t binding,
	const ImageDX12& image,
	D3D12_SRV_DIMENSION dimension,
	DXGI_FORMAT viewFormat
)
{
	if (!image.valid())
	{
		throw std::runtime_error("DescriptorSetDX12::writeTextureSRV - invalid image");
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.Format = viewFormat == DXGI_FORMAT_UNKNOWN
		? image.srvFormat()
		: viewFormat;

	desc.ViewDimension = dimension;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (dimension == D3D12_SRV_DIMENSION_TEXTURE2D)
	{
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = image.mipLevels();
		desc.Texture2D.PlaneSlice = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	else if (dimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
	{
		desc.Texture2DArray.MostDetailedMip = 0;
		desc.Texture2DArray.MipLevels = image.mipLevels();
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = image.layers();
		desc.Texture2DArray.PlaneSlice = 0;
		desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	}
	else if (dimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
	{
		desc.TextureCube.MostDetailedMip = 0;
		desc.TextureCube.MipLevels = image.mipLevels();
		desc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		throw std::runtime_error("DescriptorSetDX12::writeTextureSRV - unsupported dimension");
	}

	dx_->getDevice()->CreateShaderResourceView(
		image.resource(),
		&desc,
		getCPUHandle(binding)
	);
} // end of writeTextureSRV()

void DescriptorSetDX12::writeStorageImageUAV(
	uint32_t binding,
	const ImageDX12& image,
	D3D12_UAV_DIMENSION dimension,
	DXGI_FORMAT viewFormat
)
{
	if (!image.valid())
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageImageUAV - invalid image");
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = viewFormat == DXGI_FORMAT_UNKNOWN
		? image.format()
		: viewFormat;

	desc.ViewDimension = dimension;

	if (dimension == D3D12_UAV_DIMENSION_TEXTURE2D)
	{
		desc.Texture2D.MipSlice = 0;
		desc.Texture2D.PlaneSlice = 0;
	}
	else if (dimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
	{
		desc.Texture2DArray.MipSlice = 0;
		desc.Texture2DArray.FirstArraySlice = 0;
		desc.Texture2DArray.ArraySize = image.layers();
		desc.Texture2DArray.PlaneSlice = 0;
	}
	else
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageImageUAV - unsupported dimension");
	}

	dx_->getDevice()->CreateUnorderedAccessView(
		image.resource(),
		nullptr,
		&desc,
		getCPUHandle(binding)
	);
} // end of writeStorageImageUAV()

void DescriptorSetDX12::writeTextureSRVAtSlot(
	uint32_t slot,
	const ImageDX12& image,
	D3D12_SRV_DIMENSION dimension,
	DXGI_FORMAT viewFormat,
	uint32_t mostDetailedMip,
	uint32_t mipCount
)
{
	if (!image.valid())
	{
		throw std::runtime_error("DescriptorSetDX12::writeTextureSRVAtSlot - invalid image");
	}

	if (mostDetailedMip >= image.mipLevels())
	{
		throw std::runtime_error("DescriptorSetDX12::writeTextureSRVAtSlot - invalid mip");
	}

	const UINT finalMipCount =
		mipCount == UINT32_MAX
		? image.mipLevels() - mostDetailedMip
		: std::min(mipCount, image.mipLevels() - mostDetailedMip);

	D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.Format = viewFormat == DXGI_FORMAT_UNKNOWN
		? image.srvFormat()
		: viewFormat;

	desc.ViewDimension = dimension;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (dimension == D3D12_SRV_DIMENSION_TEXTURE2D)
	{
		desc.Texture2D.MostDetailedMip = mostDetailedMip;
		desc.Texture2D.MipLevels = finalMipCount;
		desc.Texture2D.PlaneSlice = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		throw std::runtime_error("DescriptorSetDX12::writeTextureSRVAtSlot - unsupported dimension");
	}

	dx_->getDevice()->CreateShaderResourceView(
		image.resource(),
		&desc,
		getCPUHandleAtSlot(slot)
	);
} // end of writeTextureSRVAtSlot()

void DescriptorSetDX12::writeStorageImageUAVAtSlot(
	uint32_t slot,
	const ImageDX12& image,
	D3D12_UAV_DIMENSION dimension,
	DXGI_FORMAT viewFormat,
	uint32_t mipSlice
)
{
	if (!image.valid())
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageImageUAVAtSlot - invalid image");
	}

	if (mipSlice >= image.mipLevels())
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageImageUAVAtSlot - invalid mip");
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
	desc.Format = viewFormat == DXGI_FORMAT_UNKNOWN
		? image.format()
		: viewFormat;

	desc.ViewDimension = dimension;

	if (dimension == D3D12_UAV_DIMENSION_TEXTURE2D)
	{
		desc.Texture2D.MipSlice = mipSlice;
		desc.Texture2D.PlaneSlice = 0;
	}
	else
	{
		throw std::runtime_error("DescriptorSetDX12::writeStorageImageUAVAtSlot - unsupported dimension");
	}

	dx_->getDevice()->CreateUnorderedAccessView(
		image.resource(),
		nullptr,
		&desc,
		getCPUHandleAtSlot(slot)
	);
} // end of writeStorageImageUAVAtSlot()

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorSetDX12::getGPUHandle(uint32_t binding) const
{
	if (!descriptorHeap_)
	{
		throw std::runtime_error("DescriptorSetDX12::getGPUHandle - descriptor heap not created");
	}

	D3D12_GPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

	handle.ptr += getSlot(binding) * descriptorSize_;

	return handle;
} // end of getGPUHandle()

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorSetDX12::getCPUHandle(uint32_t binding) const
{
	if (!descriptorHeap_)
	{
		throw std::runtime_error("DescriptorSetDX12::getCPUHandle - descriptor heap not created");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	handle.ptr += getSlot(binding) * descriptorSize_;

	return handle;
} // end of getCPUHandle()

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorSetDX12::getCPUHandleAtSlot(uint32_t slot) const
{
	if (!descriptorHeap_)
	{
		throw std::runtime_error("DescriptorSetDX12::getCPUHandleAtSlot - descriptor heap not created");
	}

	if (slot >= descriptorCount_)
	{
		throw std::runtime_error("DescriptorSetDX12::getCPUHandleAtSlot - slot out of range");
	}

	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	handle.ptr += static_cast<SIZE_T>(slot) * descriptorSize_;

	return handle;
} // end of getCPUHandleAtSlot()

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorSetDX12::getGPUHandleAtSlot(uint32_t slot) const
{
	if (!descriptorHeap_)
	{
		throw std::runtime_error("DescriptorSetDX12::getGPUHandleAtSlot - descriptor heap not created");
	}

	if (slot >= descriptorCount_)
	{
		throw std::runtime_error("DescriptorSetDX12::getGPUHandleAtSlot - slot out of range");
	}

	D3D12_GPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

	handle.ptr += static_cast<UINT64>(slot) * descriptorSize_;

	return handle;
} // end of getGPUHandleAtSlot()

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorSetDX12::getTableGPUHandle() const
{
	if (!descriptorHeap_)
	{
		return {};
	}

	return descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
} // end of getTableGPUHandle()


//--- PRIVATE ---//
uint32_t DescriptorSetDX12::getSlot(uint32_t binding) const
{
	auto it = bindingToSlot_.find(binding);

	if (it == bindingToSlot_.end())
	{
		throw std::runtime_error("DescriptorSetDX12::getSlot - binding not found");
	}

	return it->second;
} // end of getSlot()

uint32_t DescriptorSetDX12::getPushConstantRootIndex(uint32_t binding) const
{
	auto it = pushConstantBindingToRootIndex_.find(binding);

	if (it == pushConstantBindingToRootIndex_.end())
	{
		throw std::runtime_error("DescriptorSetDX12::getPushConstantRootIndex - binding not found");
	}

	return it->second;
} // end of getPushConstantRootIndex()
