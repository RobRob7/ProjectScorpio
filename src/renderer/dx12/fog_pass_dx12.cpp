#include "fog_pass_dx12.h"

#include "render_settings.h"

#include "constants.h"

#include "bindings.h"
#include "compute_shader_dx12.h"
#include "dx12_main.h"
#include "image_dx12.h"

#include <cstdint>
#include <algorithm>

//--- PUBLIC ---//
FogPassDX12::FogPassDX12(
	DX12Main& dx, 
	const RenderSettings& rs
)
	: dx_(&dx),
	rs_(rs),
	outputImage_(dx),
	pipeline_(dx)
{
	factor_ = std::max(1u, rs_.resScale.FOG);

	const uint32_t frames = dx.getMaxFramesInFlight();

	uboBuffers_.reserve(frames);
	descriptorSets_.reserve(frames);
	for (uint32_t i = 0; i < frames; ++i)
	{
		uboBuffers_.emplace_back(dx);
		descriptorSets_.emplace_back(dx);
	} // end for
} // end of constuctor

FogPassDX12::~FogPassDX12() = default;

void FogPassDX12::init()
{
	Extent2D extent = dx_->getSwapChainExtent();
	width_ = std::max(1u, (extent.width + factor_ - 1) / factor_);
	height_ = std::max(1u, (extent.height + factor_ - 1) / factor_);

	workGroupX_ = (width_ + numWorkGroups_ - 1) / numWorkGroups_;
	workGroupY_ = (height_ + numWorkGroups_ - 1) / numWorkGroups_;

	shader_ = std::make_unique<ComputeShaderDX12>(
		"hlsl/fogpass/fog.comp.cso"
	);

	createAttachment();
	createResources();
	createDescriptorSet();
	createPipeline();
} // end of init()

void FogPassDX12::resize()
{
	Extent2D extent = dx_->getSwapChainExtent();
	if (extent.width <= 0 || extent.height <= 0) return;

	uint32_t newWidth = (extent.width + factor_ - 1) / factor_;
	uint32_t newHeight = (extent.height + factor_ - 1) / factor_;

	if (newWidth == width_ && newHeight == height_) return;

	width_ = newWidth;
	height_ = newHeight;

	workGroupX_ = (width_ + (numWorkGroups_ - 1)) / numWorkGroups_;
	workGroupY_ = (height_ + (numWorkGroups_ - 1)) / numWorkGroups_;

	const uint32_t retireFrame = dx_->getPrevFrameIndex();

	dx_->retireImage(retireFrame, std::move(outputImage_));
	outputImage_ = ImageDX12(*dx_);

	createAttachment();
	updateDescriptorSet(dx_->currentFrameIndex());
} // end of resize()

void FogPassDX12::render(
	const FogPassUBOs& ubos, 
	const FrameContextDX12& frame
)
{
	syncSettings();

	if (!inputDepthImage_ ||
		!outputImage_.valid() ||
		!pipeline_.valid())
	{
		return;
	}

	updateDescriptorSet(frame.frameIndex);
	
	ID3D12GraphicsCommandList* cmd = frame.cmd;

	cmd->SetName({ L"FogPassDX12::cmd" });

	outputImage_.transitionToUnorderedAccess(cmd);

	uboBuffers_[frame.frameIndex].upload(&ubos.ubo, sizeof(ubos.ubo));

	DescriptorSetDX12& set = descriptorSets_[frame.frameIndex];

	ID3D12DescriptorHeap* heaps =
	{
		set.getDescriptorHeap()
	};
	cmd->SetDescriptorHeaps(1, &heaps);

	cmd->SetComputeRootSignature(set.getRootSignature());
	cmd->SetPipelineState(pipeline_.getPipeline());

	cmd->SetComputeRootDescriptorTable(
		set.getDescriptorTableRootIndex(),
		set.getTableGPUHandle()
	);

	cmd->Dispatch(
		workGroupX_, 
		workGroupY_, 
		1
	);

	outputImage_.transitionToShaderRead(cmd);
} // end of render()


//--- PRIVATE ---//
void FogPassDX12::syncSettings()
{
	uint32_t newFactor = std::max(1u, rs_.resScale.FOG);

	if (newFactor == factor_)
		return;

	factor_ = newFactor;
	resize();
} // end of syncSettings()

void FogPassDX12::updateDescriptorSet(uint32_t frameIndex)
{
	DescriptorSetDX12& set = descriptorSets_[frameIndex];
	if (!set.valid())
	{
		return;
	}

	if (outputImage_.valid())
	{
		set.writeStorageImageUAV(
			TO_API_FORM(FogPassBinding::OutColorTex),
			outputImage_
		);
	}

	if (inputDepthImage_ && inputDepthImage_->valid())
	{
		set.writeTextureSRV(
			TO_API_FORM(FogPassBinding::ForwardDepthTex),
			*inputDepthImage_
		);
	}
} // end of updateDescriptorSet()

void FogPassDX12::createAttachment()
{
	outputImage_.createImage(
		width_,
		height_,
		1,
		false,
		outputFormat_,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	outputImage_.setDebugName(L"FogPassDX12-OutputImage");
} // end of createAttachment()

void FogPassDX12::createResources()
{
	for (auto& buffer : uboBuffers_)
	{
		buffer.create(
			sizeof(Fog_Constants::FogPassUBO),
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_FLAG_NONE,
			true
		);
	} // end for
} // end of createResources()

void FogPassDX12::createDescriptorSet()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		DescriptorBindingDX12 uboBinding{
			.binding = TO_API_FORM(FogPassBinding::UBO),
			.type = DescriptorTypeDX12::UniformBuffer,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_ALL
		};
		DescriptorBindingDX12 inputDepthBinding{
			.binding = TO_API_FORM(FogPassBinding::ForwardDepthTex),
			.type = DescriptorTypeDX12::TextureSRV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_ALL
		};
		DescriptorBindingDX12 outputColorBinding{
			.binding = TO_API_FORM(FogPassBinding::OutColorTex),
			.type = DescriptorTypeDX12::StorageImageUAV,
			.count = 1,
			.visibility = D3D12_SHADER_VISIBILITY_ALL
		};

		descriptorSets_[i].createLayout({
			uboBinding,
			inputDepthBinding,
			outputColorBinding
			});
		descriptorSets_[i].createPool(3);
		descriptorSets_[i].allocate();

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(FogPassBinding::UBO),
			uboBuffers_[i],
			sizeof(Fog_Constants::FogPassUBO)
		);

		descriptorSets_[i].setDebugName(
			L"FogPassDX12::DescriptorSet frame " + std::to_wstring(i)
		);
	} // end for
} // end of createDescriptorSet()

void FogPassDX12::createPipeline()
{
	ComputePipelineDescDX12 compDesc{
		.computeShader = shader_->computeShader(),

		.rootSignature = descriptorSets_[0].getRootSignature()
	};

	pipeline_.create(compDesc);
	pipeline_.setDebugName(L"FogPassDX12::Pipeline");
} // end of createPipeline()