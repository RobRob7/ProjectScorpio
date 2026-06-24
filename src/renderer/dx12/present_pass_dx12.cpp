#include "present_pass_dx12.h"

#include "frame_context_dx12.h"

#include "bindings.h"
#include "shader_dx12.h"
#include "dx12_main.h"
#include "image_dx12.h"

#include <cstdint>

//--- PUBLIC ---//
PresentPassDX12::PresentPassDX12(DX12Main& dx)
	: dx_(&dx),
	pipeline_(dx)
{
    const uint32_t frames = dx_->getMaxFramesInFlight();

    descriptorSets_.reserve(frames);
    for (uint32_t i = 0; i < frames; ++i)
    {
        descriptorSets_.emplace_back(*dx_);
    } // end for
} // end of constructor

PresentPassDX12::~PresentPassDX12() = default;

void PresentPassDX12::init()
{
    shader_ = std::make_unique<ShaderDX12>(
        "hlsl/presentpass/present.vert.cso",
        "hlsl/presentpass/present.frag.cso"
    );

    createDescriptorSets();
    createPipeline();
} // end of init()

void PresentPassDX12::resize()
{
    refreshInput();
} // end of resize()

void PresentPassDX12::render(FrameContextDX12& frame)
{
    if (!inputImage_ || 
        !pipeline_.valid() ||
        !descriptorSets_[frame.frameIndex].valid())
    {
        return;
    }

    ID3D12GraphicsCommandList* cmd = frame.cmd;

    D3D12_VIEWPORT viewport{
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<float>(frame.width),
        .Height = static_cast<float>(frame.height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f
    };
    D3D12_RECT scissor{
        .left = 0,
        .top = 0,
        .right = static_cast<LONG>(frame.width),
        .bottom = static_cast<LONG>(frame.height)
    };
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);

    D3D12_CPU_DESCRIPTOR_HANDLE colorRTV = frame.colorRTV;
    cmd->OMSetRenderTargets(
        1,
        &colorRTV,
        FALSE,
        nullptr
    );

    const float clearColor[4] =
    {
        0.0f, 0.0f, 0.0f, 1.0f
    };
    cmd->ClearRenderTargetView(
        colorRTV,
        clearColor,
        0,
        nullptr
    );

    ID3D12DescriptorHeap* heaps[] =
    {
        descriptorSets_[frame.frameIndex].getDescriptorHeap()
    };

    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootSignature(pipeline_.getRootSignature());
    cmd->SetPipelineState(pipeline_.getPipeline());

    cmd->SetGraphicsRootDescriptorTable(
        0,
        descriptorSets_[frame.frameIndex].getTableGPUHandle()
    );

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);
} // end of render()


//--- PRIVATE ---//
void PresentPassDX12::refreshInput()
{
    if (!inputImage_)
    {
        return;
    }

    for (auto& set : descriptorSets_)
    {
        if (!set.valid())
        {
            continue;
        }

        set.writeTextureSRV(
            TO_API_FORM(PresentPassBinding::ForwardColorTex),
            *inputImage_,
            D3D12_SRV_DIMENSION_TEXTURE2D
        );
    } // end for
} // end of refreshInput()

void PresentPassDX12::createDescriptorSets()
{
    const uint32_t frames = dx_->getMaxFramesInFlight();

    for (uint32_t i = 0; i < frames; ++i)
    {
        DescriptorBindingDX12 inputTexBinding{
            .binding = TO_API_FORM(PresentPassBinding::ForwardColorTex),
            .type = DescriptorTypeDX12::TextureSRV,
            .count = 1,
            .visibility = D3D12_SHADER_VISIBILITY_PIXEL
        };

        descriptorSets_[i].createLayout({
            inputTexBinding
            });

        descriptorSets_[i].createPool(1);
        descriptorSets_[i].allocate();

        descriptorSets_[i].setDebugName(
            L"PresentPassDX12::DescriptorSet frame " + std::to_wstring(i)
        );
    } // end for
} // end of createDescriptorSet()

void PresentPassDX12::createPipeline()
{
    GraphicsPipelineDescDX12 desc{
        .vertShader = shader_->vertShader(),
        .fragShader = shader_->fragShader(),

        .rootSignature = descriptorSets_[0].getRootSignature(),

        .cullMode = D3D12_CULL_MODE_NONE,
        .frontCCW = FALSE,

        .depthTestEnable = FALSE,
        .depthWriteEnable = FALSE,

        .colorFormat = dx_->getSwapChainImageFormat()
    };

    pipeline_.create(desc);
    pipeline_.setDebugName(L"PresentPassDX12::Pipeline");
} // end of createPipeline()