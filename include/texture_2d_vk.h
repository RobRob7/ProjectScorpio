#ifndef TEXTURE_2D_VK_H
#define TEXTURE_2D_VK_H

#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <string_view>
#include <cstdint>
#include <string>

class VulkanMain;

class Texture2DVk
{
public:
    explicit Texture2DVk(VulkanMain& vk);
    ~Texture2DVk();

    Texture2DVk(const Texture2DVk&) = delete;
    Texture2DVk& operator=(const Texture2DVk&) = delete;

    Texture2DVk(Texture2DVk&&) noexcept = default;
    Texture2DVk& operator=(Texture2DVk&&) noexcept = default;

    void setDebugName(const std::string& name);

    void loadFromFile(std::string_view path, const bool needToFlip = false);

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
