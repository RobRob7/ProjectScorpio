#include "post_composite_pass_dx12.h"

#include "frame_context_dx12.h"

#include "render_settings.h"

#include "bindings.h"
#include "compute_shader_dx12.h"
#include "dx12_main.h"
#include "image_dx12.h"

#include <cstdint>
#include <algorithm>

//--- PUBLIC ---//
PostCompositePassDX12::PostCompositePassDX12(DX12Main& dx)
    : dx_(&dx),
    postColorImage_(dx),
    pipeline_(dx)
{
    const uint32_t frames = dx.getMaxFramesInFlight();

    descriptorSets_.reserve(frames);
    for (uint32_t i = 0; i < frames; ++i)
    {
        descriptorSets_.emplace_back(dx);
    } // end for
} // end of constructor

PostCompositePassDX12::~PostCompositePassDX12() = default;

void PostCompositePassDX12::init()
{
    Extent2D extent = dx_->getSwapChainExtent();

    workGroupX_ = (extent.width + numWorkGroups_ - 1) / numWorkGroups_;
    workGroupY_ = (extent.height + numWorkGroups_ - 1) / numWorkGroups_;

    shader_ = std::make_unique<ComputeShaderDX12>(
        "hlsl/compositepass/post_compositepass.comp.cso"
    );

    createAttachment();
    createDescriptorSet();
    createPipeline();
} // end of init()

void PostCompositePassDX12::resize()
{
    Extent2D extent = dx_->getSwapChainExtent();
    if (extent.width <= 0 || extent.height <= 0) return;

    workGroupX_ = (extent.width + (numWorkGroups_ - 1)) / numWorkGroups_;
    workGroupY_ = (extent.height + (numWorkGroups_ - 1)) / numWorkGroups_;

    const uint32_t retireFrame = dx_->getPrevFrameIndex();

    dx_->retireImage(retireFrame, std::move(postColorImage_));
    postColorImage_ = ImageDX12(*dx_);

    createAttachment();
    updateDescriptorSet(dx_->currentFrameIndex());
} // end of resize()

void PostCompositePassDX12::render(FrameContextDX12& frame)
{
    if (!fogColorImage_ ||
        !godRayColorImage_ ||
        !sceneColorImage_ ||
        !pipeline_.valid())
    {
        return;
    }

    updateDescriptorSet(frame.frameIndex);

    ID3D12GraphicsCommandList* cmd = frame.cmd;

    cmd->SetName({ L"PostCompositePassDX12::cmd" });

    fogColorImage_->transitionToShaderRead(cmd, false);
    godRayColorImage_->transitionToShaderRead(cmd, false);
    sceneColorImage_->transitionToShaderRead(cmd, false);

    postColorImage_.transitionToUnorderedAccess(cmd);

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

    postColorImage_.transitionToShaderRead(cmd);
} // end of render()


//--- PRIVATE ---//
void PostCompositePassDX12::updateDescriptorSet(uint32_t frameIndex)
{
    DescriptorSetDX12& set = descriptorSets_[frameIndex];
    if (!set.valid())
    {
        return;
    }

    if (postColorImage_.valid())
    {
        set.writeStorageImageUAV(
            TO_API_FORM(PostCompositePassBinding::PostOutColorTex),
            postColorImage_
        );
    }

    if (fogColorImage_ &&  fogColorImage_->valid())
    {
        set.writeTextureSRV(
            TO_API_FORM(PostCompositePassBinding::FogTex),
            *fogColorImage_
        );
    }

    if (godRayColorImage_ && godRayColorImage_->valid())
    {
        set.writeTextureSRV(
            TO_API_FORM(PostCompositePassBinding::GodRayTex),
            *godRayColorImage_
        );
    }

    if (sceneColorImage_ && sceneColorImage_->valid())
    {
        set.writeTextureSRV(
            TO_API_FORM(PostCompositePassBinding::SceneColorTex),
            *sceneColorImage_
        );
    }
} // end of updateDescriptorSet()

void PostCompositePassDX12::createAttachment()
{
    Extent2D extent = dx_->getSwapChainExtent();

    postColorImage_.createImage(
        extent.width,
        extent.height,
        1,
        false,
        postColorFormat_,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    postColorImage_.setDebugName(L"PostCompositePassDX12-ColorImage");
} // end of createAttachment()

void PostCompositePassDX12::createDescriptorSet()
{
    const uint32_t frames = dx_->getMaxFramesInFlight();

    for (uint32_t i = 0; i < frames; ++i)
    {
        DescriptorBindingDX12 fogColorBinding{
            .binding = TO_API_FORM(PostCompositePassBinding::FogTex),
            .type = DescriptorTypeDX12::TextureSRV,
            .count = 1,
            .visibility = D3D12_SHADER_VISIBILITY_ALL
        };
        DescriptorBindingDX12 godRayColorBinding{
            .binding = TO_API_FORM(PostCompositePassBinding::GodRayTex),
            .type = DescriptorTypeDX12::TextureSRV,
            .count = 1,
            .visibility = D3D12_SHADER_VISIBILITY_ALL
        };
        DescriptorBindingDX12 sceneColorBinding{
            .binding = TO_API_FORM(PostCompositePassBinding::SceneColorTex),
            .type = DescriptorTypeDX12::TextureSRV,
            .count = 1,
            .visibility = D3D12_SHADER_VISIBILITY_ALL
        };
        DescriptorBindingDX12 postColorOutBinding{
            .binding = TO_API_FORM(PostCompositePassBinding::PostOutColorTex),
            .type = DescriptorTypeDX12::StorageImageUAV,
            .count = 1,
            .visibility = D3D12_SHADER_VISIBILITY_ALL
        };

        descriptorSets_[i].createLayout({
            fogColorBinding,
            godRayColorBinding,
            sceneColorBinding,
            postColorOutBinding
            });

        descriptorSets_[i].createPool(4);
        descriptorSets_[i].allocate();

        descriptorSets_[i].setDebugName(
            L"PostCompositePassDX12::DescriptorSet frame " + std::to_wstring(i)
        );
    } // end for
} // end of createDescriptorSet()

void PostCompositePassDX12::createPipeline()
{
    ComputePipelineDescDX12 compDesc{
        .computeShader = shader_->computeShader(),

        .rootSignature = descriptorSets_[0].getRootSignature()
    };

    pipeline_.create(compDesc);

    pipeline_.setDebugName(L"PostCompositePassDX12::Pipeline");
} // end of createPipeline()