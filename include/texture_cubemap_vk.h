#ifndef TEXTURE_CUBEMAP_VK_H
#define TEXTURE_CUBEMAP_VK_H

#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <string_view>
#include <cstdint>
#include <string>

class VulkanMain;

class TextureCubemapVk
{
public:
    explicit TextureCubemapVk(VulkanMain& vk);
    ~TextureCubemapVk();

    TextureCubemapVk(const TextureCubemapVk&) = delete;
    TextureCubemapVk& operator=(const TextureCubemapVk&) = delete;

    TextureCubemapVk(TextureCubemapVk&&) noexcept = default;
    TextureCubemapVk& operator=(TextureCubemapVk&&) noexcept = default;

    void setDebugName(const std::string& name);

    void loadFromFiles(const std::array<std::string_view, 6>& faces, const bool needToFlip = false);

    bool valid() const { return image_.valid(); }

    vk::Image image() const { return image_.image(); }
    vk::ImageView view() const { return image_.view(); }
    vk::Sampler sampler() const { return image_.sampler(); }

    uint32_t width() const { return image_.width(); }
    uint32_t height() const { return image_.height(); }
    vk::Format format() const { return image_.format(); }

private:
    void destroy();
private:
    VulkanMain& vk_;
    ImageVk image_;
};

#endif
