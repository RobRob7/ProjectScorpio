#include "present_pass_vk.h"

#include "frame_context_vk.h"

#include "bindings.h"
#include "shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

//--- PUBLIC ---//
PresentPassVk::PresentPassVk(VulkanMain& vk)
	: vk_(vk),
	pipeline_(vk)
{
    descriptorSets_.reserve(vk.getMaxFramesInFlight());
    for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
    {
        descriptorSets_.emplace_back(vk_);
    } // end for
} // end of constructor

PresentPassVk::~PresentPassVk() = default;

void PresentPassVk::init()
{
    shader_ = std::make_unique<ShaderModuleVk>(
        vk_.getDevice(),
        "presentpass/present.vert.spv",
        "presentpass/present.frag.spv"
    );

    createDescriptorSets();
    createPipeline();
} // end of init()

void PresentPassVk::resize()
{
    refreshInput();
} // end of resize()

void PresentPassVk::render(FrameContext& frame)
{
    if (!inputImage_ || !pipeline_.valid())
    {
        return;
    }

    DescriptorSetVk& desc = descriptorSets_[frame.frameIndex];
    if (!desc.valid()) return;

    desc.writeCombinedImageSampler(
        TO_API_FORM(PresentPassBinding::ForwardColorTex),
        inputImage_->view(),
        inputImage_->sampler()
    );

    vk::DescriptorSet set = desc.getSet();

    vk::CommandBuffer cmd = frame.cmd;

    vk::ClearValue clear{ {0.0f, 0.0f, 0.0f, 1.0f} };

    vk::RenderingAttachmentInfo colorAttach{};
    colorAttach.imageView = frame.colorImageView;
    colorAttach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttach.loadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttach.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttach.clearValue = clear;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
    renderingInfo.renderArea.extent = frame.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttach;
    renderingInfo.pDepthAttachment = nullptr;

    cmd.beginRendering(renderingInfo);
    {
        vk::Viewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(frame.extent.width);
        viewport.height = static_cast<float>(frame.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmd.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{ 0, 0 };
        scissor.extent = frame.extent;
        cmd.setScissor(0, 1, &scissor);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline_.getLayout(),
            0,
            1, &set,
            0, nullptr
        );

        cmd.draw(3, 1, 0, 0);
    }
    cmd.endRendering();
} // end of render()


//--- PRIVATE ---//
void PresentPassVk::refreshInput()
{
    if (!inputImage_)
        return;

    for (auto& set : descriptorSets_)
    {
        set.writeCombinedImageSampler(
            TO_API_FORM(PresentPassBinding::ForwardColorTex),
            inputImage_->view(),
            inputImage_->sampler()
        );
    } // end for
} // end of refreshInput()

void PresentPassVk::createDescriptorSets()
{
    for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
    {
        vk::DescriptorSetLayoutBinding inputBinding{};
        inputBinding.binding = TO_API_FORM(PresentPassBinding::ForwardColorTex);
        inputBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        inputBinding.descriptorCount = 1;
        inputBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        descriptorSets_[i].createLayout({
            inputBinding
            });

        vk::DescriptorPoolSize inputPool{};
        inputPool.type = vk::DescriptorType::eCombinedImageSampler;
        inputPool.descriptorCount = 1;

        descriptorSets_[i].createPool({ 
            inputPool 
            });
        descriptorSets_[i].allocate();

        descriptorSets_[i].setDebugName(
            "PresentPassVk::descriptorSets_ frame " + std::to_string(i)
        );
    } // end for
} // end of createDescriptorSet()

void PresentPassVk::createPipeline()
{
    GraphicsPipelineDescVk desc{};
    desc.vertShader = shader_->vertShader();
    desc.fragShader = shader_->fragShader();

    desc.setLayouts = { descriptorSets_[0].getLayout()};

    desc.colorFormat = vk_.getSwapChainImageFormat();

    desc.cullMode = vk::CullModeFlagBits::eNone;
    desc.frontFace = vk::FrontFace::eClockwise;
    desc.depthTestEnable = false;
    desc.depthWriteEnable = false;

    pipeline_.create(desc);
} // end of createPipeline()