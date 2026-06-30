#include "mip_gen_dx12.h"

#include "dx12_main.h"
#include "compute_shader_dx12.h"
#include "image_dx12.h"

#include <cstdint>

//--- PUBLIC ---//
MipGenDX12::MipGenDX12(DX12Main& dx)
	: dx_(&dx),
	pipeline_(dx)
{
	const uint32_t frames = dx.getMaxFramesInFlight();

	descriptorSets_.reserve(frames);
	for (uint32_t i = 0; i < frames; ++i)
	{
		descriptorSets_.emplace_back(dx);
	} // end for

	shader_ = std::make_unique<ComputeShaderDX12>(
		"hlsl/utility/mip_gen.comp.cso"
	);

	createDescriptorSets();
	createPipeline();
} // end of constructor

MipGenDX12::~MipGenDX12() = default;

void MipGenDX12::generate(
	ImageDX12& image,
	ID3D12GraphicsCommandList* cmd
)
{
	if (!image.valid())
	{
		return;
	}

	DescriptorSetDX12& set = descriptorSets_[dx_->currentFrameIndex()];

	ID3D12DescriptorHeap* heap = set.getDescriptorHeap();
	cmd->SetDescriptorHeaps(1, &heap);

	cmd->SetComputeRootSignature(set.getRootSignature());
	cmd->SetPipelineState(pipeline_.getPipeline());

	for (uint32_t mip = 1; mip < image.mipLevels(); ++mip)
	{
		dx_->beginGPUEvent(cmd, L"MipGen::MipLevel");
		const uint32_t pairSlot = (mip - 1) * 2;
		const uint32_t srvSlot = pairSlot + 0;
		const uint32_t uavSlot = pairSlot + 1;

		if (mip == 1)
		{
			image.transitionSubresource(
				cmd,
				0,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
			);
		}

		image.transitionSubresource(
			cmd,
			mip,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);

		set.writeTextureSRVAtSlot(
			srvSlot,
			image,
			D3D12_SRV_DIMENSION_TEXTURE2D,
			image.srvFormat(),
			mip - 1,
			1
		);

		set.writeStorageImageUAVAtSlot(
			uavSlot,
			image,
			D3D12_UAV_DIMENSION_TEXTURE2D,
			image.format(),
			mip
		);

		cmd->SetComputeRootDescriptorTable(
			set.getDescriptorTableRootIndex(),
			set.getGPUHandleAtSlot(pairSlot)
		);

		const uint32_t dstWidth = image.getMipWidth(mip);
		const uint32_t dstHeight = image.getMipHeight(mip);

		cmd->Dispatch(
			(dstWidth + 15) / 16,
			(dstHeight + 15) / 16,
			1
		);

		D3D12_RESOURCE_BARRIER uavBarrier{};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavBarrier.UAV.pResource = image.resource();

		cmd->ResourceBarrier(1, &uavBarrier);

		image.transitionSubresource(
			cmd,
			mip,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		);

		dx_->endGPUEvent(cmd);
	} // end for
} // end of generate()


//--- PRIVATE ---//
void MipGenDX12::createDescriptorSets()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		DescriptorBindingDX12 srcMipBinding{
			.binding = static_cast<uint32_t>(0),
			.type = DescriptorTypeDX12::TextureSRV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_ALL
		};
		DescriptorBindingDX12 dstMipBinding{
			.binding = static_cast<uint32_t>(1),
			.type = DescriptorTypeDX12::StorageImageUAV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_ALL
		};

		descriptorSets_[i].createLayout({
			srcMipBinding,
			dstMipBinding
		});

		descriptorSets_[i].createPool(64);
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			L"MipGenDX12::DescriptorSet frame " + std::to_wstring(i)
		);
	} // end for
} // end of createDescriptorSets()

void MipGenDX12::createPipeline()
{
	ComputePipelineDescDX12 desc{
		.computeShader = shader_->computeShader(),

		.rootSignature = descriptorSets_[0].getRootSignature()
	};

	pipeline_.create(desc);
	pipeline_.setDebugName(L"MipGenDX12::Pipeline");
} // end of createPipeline()