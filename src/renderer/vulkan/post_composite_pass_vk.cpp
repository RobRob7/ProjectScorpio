#include "post_composite_pass_vk.h"

#include "frame_context_vk.h"

#include "bindings.h"
#include "shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

//--- PUBLIC ---//
PostCompositePassVk::PostCompositePassVk(VulkanMain& vk)
    : vk_(vk),
    postColorImage_(vk),
    pipeline_(vk)
{
    descriptorSets_.reserve(vk.getMaxFramesInFlight());
    for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
    {
        descriptorSets_.emplace_back(vk_);
    } // end for
} // end of constructor

PostCompositePassVk::~PostCompositePassVk() = default;

void PostCompositePassVk::init()
{
    shader_ = std::make_unique<ShaderModuleVk>(
        vk_.getDevice(),
        "compositepass/post_compositepass.vert.spv",
        "compositepass/post_compositepass.frag.spv"
    );

    createAttachment();
    createDescriptorSet();
    createPipeline();
} // end of init()

void PostCompositePassVk::resize()
{
    postColorImage_.destroy();
    createAttachment();
    refreshInput();
} // end of resize()

void PostCompositePassVk::render(FrameContext& frame)
{
    if (!fogColorImage_ ||
        !sceneColorImage_ ||
        !pipeline_.valid())
    {
        return;
    }

    DescriptorSetVk& desc = descriptorSets_[frame.frameIndex];
    if (!desc.valid()) return;

    desc.writeCombinedImageSampler(
        TO_API_FORM(PostCompositePassBinding::FogColorTex),
        fogColorImage_->view(),
        fogColorImage_->sampler()
    );
    desc.writeCombinedImageSampler(
        TO_API_FORM(PostCompositePassBinding::SceneColorTex),
        sceneColorImage_->view(),
        sceneColorImage_->sampler()
    );

    vk::DescriptorSet set = desc.getSet();

    vk::CommandBuffer cmd = frame.cmd;

    cmd.beginDebugUtilsLabelEXT({ "PostCompositePassVk::cmd" });

    postColorImage_.transitionToColorAttachment(cmd);

    vk::ClearValue colorClear{ {0.0f, 0.0f, 0.0f, 1.0f} };

    vk::RenderingAttachmentInfo colorAttach{};
    colorAttach.imageView = postColorImage_.view();
    colorAttach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttach.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttach.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttach.clearValue = colorClear;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
    renderingInfo.renderArea.extent = frame.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttach;

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

    postColorImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eColor);

    cmd.endDebugUtilsLabelEXT();
} // end of render()


//--- PRIVATE ---//
void PostCompositePassVk::refreshInput()
{
    if (!fogColorImage_ ||
        !sceneColorImage_)
        return;

    for (auto& set : descriptorSets_)
    {
        set.writeCombinedImageSampler(
            TO_API_FORM(PostCompositePassBinding::FogColorTex),
            fogColorImage_->view(),
            fogColorImage_->sampler()
        );
        set.writeCombinedImageSampler(
            TO_API_FORM(PostCompositePassBinding::SceneColorTex),
            sceneColorImage_->view(),
            sceneColorImage_->sampler()
        );
    } // end for
} // end of refreshInput()

void PostCompositePassVk::createAttachment()
{
    vk::Extent2D extent = vk_.getSwapChainExtent();

    // color
    postColorImage_.createImage(
        extent.width,
        extent.height,
        1,
        false,
        vk::SampleCountFlagBits::e1,
        postColorFormat_,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    postColorImage_.createImageView(
        postColorFormat_,
        vk::ImageAspectFlagBits::eColor,
        vk::ImageViewType::e2D,
        1
    );

    postColorImage_.createSampler(
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge,
        false
    );

    postColorImage_.setDebugName("PostCompositePassVk-ColorImage");
} // end of createAttachment()

void PostCompositePassVk::createDescriptorSet()
{
    for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
    {
        vk::DescriptorSetLayoutBinding fogColorBinding{};
        fogColorBinding.binding = TO_API_FORM(PostCompositePassBinding::FogColorTex);
        fogColorBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        fogColorBinding.descriptorCount = 1;
        fogColorBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding fxaaDepthBinding{};
        fxaaDepthBinding.binding = TO_API_FORM(PostCompositePassBinding::SceneColorTex);
        fxaaDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        fxaaDepthBinding.descriptorCount = 1;
        fxaaDepthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        descriptorSets_[i].createLayout({
            fogColorBinding,
            fxaaDepthBinding
            });

        vk::DescriptorPoolSize fogColorPool{};
        fogColorPool.type = vk::DescriptorType::eCombinedImageSampler;
        fogColorPool.descriptorCount = 1;

        vk::DescriptorPoolSize fxaaDepthPool{};
        fxaaDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
        fxaaDepthPool.descriptorCount = 1;

        descriptorSets_[i].createPool({
            fogColorPool,
            fxaaDepthPool
            });
        descriptorSets_[i].allocate();

        descriptorSets_[i].setDebugName(
            "PostCompositePassVk::DescriptorSet frame " + std::to_string(i)
        );
    } // end for
} // end of createDescriptorSet()

void PostCompositePassVk::createPipeline()
{
    GraphicsPipelineDescVk desc{};
    desc.vertShader = shader_->vertShader();
    desc.fragShader = shader_->fragShader();

    desc.setLayouts = { descriptorSets_[0].getLayout() };

    desc.colorFormat = postColorFormat_;

    desc.cullMode = vk::CullModeFlagBits::eNone;
    desc.frontFace = vk::FrontFace::eClockwise;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = true;

    pipeline_.create(desc);

    pipeline_.setDebugName("PostCompositePassVk::Pipeline");
} // end of createPipeline()