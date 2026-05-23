#include "hybrid_composite_pass_vk.h"

#include "frame_context_vk.h"

#include "bindings.h"
#include "shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

//--- PUBLIC ---//
HybridCompositePassVk::HybridCompositePassVk(VulkanMain& vk)
    : vk_(vk),
    hybridColorImage_(vk),
    hybridDepthImage_(vk),
    pipeline_(vk)
{
    descriptorSets_.reserve(vk.getMaxFramesInFlight());
    for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
    {
        descriptorSets_.emplace_back(vk_);
    } // end for
} // end of constructor

HybridCompositePassVk::~HybridCompositePassVk() = default;

void HybridCompositePassVk::init()
{
    shader_ = std::make_unique<ShaderModuleVk>(
        vk_.getDevice(),
        "compositepass/hybrid_compositepass.vert.spv",
        "compositepass/hybrid_compositepass.frag.spv"
    );

    createAttachment();
    createDescriptorSet();
    createPipeline();
} // end of init()

void HybridCompositePassVk::resize()
{
    hybridColorImage_.destroy();
    hybridDepthImage_.destroy();
    createAttachment();
    refreshInput();
} // end of resize()

void HybridCompositePassVk::render(
    FrameContext& frame,
    float nearPlane,
    float farPlane
)
{
    if (!rasterColor_ || !rasterDepth_ ||
        !rtColor_ || !rtDepth_ ||
        !pipeline_.valid())
    {
        return;
    }

    DescriptorSetVk& desc = descriptorSets_[frame.frameIndex];
    if (!desc.valid()) return;

    desc.writeCombinedImageSampler(
        TO_API_FORM(HybridCompositePassBinding::RastColorTex),
        rasterColor_->view(),
        rasterColor_->sampler()
    );
    desc.writeCombinedImageSampler(
        TO_API_FORM(HybridCompositePassBinding::RastDepthTex),
        rasterDepth_->view(),
        rasterDepth_->sampler()
    );
    desc.writeCombinedImageSampler(
        TO_API_FORM(HybridCompositePassBinding::RTColorTex),
        rtColor_->view(),
        rtColor_->sampler()
    );
    desc.writeCombinedImageSampler(
        TO_API_FORM(HybridCompositePassBinding::RTDepthTex),
        rtDepth_->view(),
        rtDepth_->sampler()
    );

    vk::DescriptorSet set = desc.getSet();

    vk::CommandBuffer cmd = frame.cmd;

    cmd.beginDebugUtilsLabelEXT({ "HybridCompositePassVk::cmd" });

    hybridColorImage_.transitionToColorAttachment(cmd);
    hybridDepthImage_.transitionToDepthAttachment(cmd);

    vk::ClearValue colorClear{ {0.0f, 0.0f, 0.0f, 1.0f} };

    vk::RenderingAttachmentInfo colorAttach{};
    colorAttach.imageView = hybridColorImage_.view();
    colorAttach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttach.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttach.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttach.clearValue = colorClear;

    vk::ClearValue depthClear{ vk::ClearDepthStencilValue{ 1.0f, 0 } };

    vk::RenderingAttachmentInfo depthAttach{};
    depthAttach.imageView = hybridDepthImage_.view();
    depthAttach.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttach.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttach.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttach.clearValue = depthClear;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
    renderingInfo.renderArea.extent = frame.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttach;
    renderingInfo.pDepthAttachment = &depthAttach;

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

    hybridColorImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eColor);
    hybridDepthImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);

    cmd.endDebugUtilsLabelEXT();
} // end of render()


//--- PRIVATE ---//
void HybridCompositePassVk::refreshInput()
{
    if (!rasterColor_ || !rasterDepth_ ||
        !rtColor_ || !rtDepth_)
        return;

    for (auto& set : descriptorSets_)
    {
        set.writeCombinedImageSampler(
            TO_API_FORM(HybridCompositePassBinding::RastColorTex),
            rasterColor_->view(),
            rasterColor_->sampler()
        );
        set.writeCombinedImageSampler(
            TO_API_FORM(HybridCompositePassBinding::RastDepthTex),
            rasterDepth_->view(),
            rasterDepth_->sampler()
        );
        set.writeCombinedImageSampler(
            TO_API_FORM(HybridCompositePassBinding::RTColorTex),
            rtColor_->view(),
            rtColor_->sampler()
        );
        set.writeCombinedImageSampler(
            TO_API_FORM(HybridCompositePassBinding::RTDepthTex),
            rtDepth_->view(),
            rtDepth_->sampler()
        );
    } // end for
} // end of refreshInput()

void HybridCompositePassVk::createAttachment()
{
    vk::Extent2D extent = vk_.getSwapChainExtent();

    // color
    hybridColorImage_.createImage(
        extent.width,
        extent.height,
        1,
        false,
        vk::SampleCountFlagBits::e1,
        hybridColorFormat_,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    hybridColorImage_.createImageView(
        hybridColorFormat_,
        vk::ImageAspectFlagBits::eColor,
        vk::ImageViewType::e2D,
        1
    );

    hybridColorImage_.createSampler(
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge,
        false
    );

    hybridColorImage_.setDebugName("HybridCompositePassVk-ColorImage");

    // depth
    hybridDepthImage_.createImage(
        extent.width,
        extent.height,
        1,
        false,
        vk::SampleCountFlagBits::e1,
        hybridDepthFormat_,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment |
        vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    hybridDepthImage_.createImageView(
        hybridDepthFormat_,
        vk::ImageAspectFlagBits::eDepth,
        vk::ImageViewType::e2D,
        1
    );

    hybridDepthImage_.createSampler(
        vk::Filter::eNearest,
        vk::Filter::eNearest,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge,
        false
    );

    hybridDepthImage_.setDebugName("HybridCompositePassVk-DepthImage");
} // end of createAttachment()

void HybridCompositePassVk::createDescriptorSet()
{
    for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
    {
        vk::DescriptorSetLayoutBinding rastColorBinding{};
        rastColorBinding.binding = TO_API_FORM(HybridCompositePassBinding::RastColorTex);
        rastColorBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        rastColorBinding.descriptorCount = 1;
        rastColorBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding rastDepthBinding{};
        rastDepthBinding.binding = TO_API_FORM(HybridCompositePassBinding::RastDepthTex);
        rastDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        rastDepthBinding.descriptorCount = 1;
        rastDepthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding rtColorBinding{};
        rtColorBinding.binding = TO_API_FORM(HybridCompositePassBinding::RTColorTex);
        rtColorBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        rtColorBinding.descriptorCount = 1;
        rtColorBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutBinding rtDepthBinding{};
        rtDepthBinding.binding = TO_API_FORM(HybridCompositePassBinding::RTDepthTex);
        rtDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        rtDepthBinding.descriptorCount = 1;
        rtDepthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        descriptorSets_[i].createLayout({
            rastColorBinding,
            rastDepthBinding,
            rtColorBinding,
            rtDepthBinding
            });

        vk::DescriptorPoolSize rastColorPool{};
        rastColorPool.type = vk::DescriptorType::eCombinedImageSampler;
        rastColorPool.descriptorCount = 1;

        vk::DescriptorPoolSize rastDepthPool{};
        rastDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
        rastDepthPool.descriptorCount = 1;

        vk::DescriptorPoolSize rtColorPool{};
        rtColorPool.type = vk::DescriptorType::eCombinedImageSampler;
        rtColorPool.descriptorCount = 1;

        vk::DescriptorPoolSize rtDepthPool{};
        rtDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
        rtDepthPool.descriptorCount = 1;

        descriptorSets_[i].createPool({
            rastColorPool,
            rastDepthPool,
            rtColorPool,
            rtDepthPool
            });
        descriptorSets_[i].allocate();

        descriptorSets_[i].setDebugName(
            "HybridCompositePassVk::DescriptorSet frame " + std::to_string(i)
        );
    } // end for
} // end of createDescriptorSet()

void HybridCompositePassVk::createPipeline()
{
    GraphicsPipelineDescVk desc{};
    desc.vertShader = shader_->vertShader();
    desc.fragShader = shader_->fragShader();

    desc.setLayouts = { descriptorSets_[0].getLayout() };

    desc.colorFormat = hybridColorFormat_;
    desc.depthFormat = hybridDepthFormat_;

    desc.cullMode = vk::CullModeFlagBits::eNone;
    desc.frontFace = vk::FrontFace::eClockwise;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = true;

    pipeline_.create(desc);

    pipeline_.setDebugName("HybridCompositePassVk::Pipeline");
} // end of createPipeline()