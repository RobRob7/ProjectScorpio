#ifndef TEXTURE_2D_DX12_H
#define TEXTURE_2D_DX12_H

#include "image_dx12.h"

#include <string_view>
#include <cstdint>
#include <string>

class DX12Main;

class Texture2DDX12
{
public:
    explicit Texture2DDX12(DX12Main& dx);
    ~Texture2DDX12();

    Texture2DDX12(const Texture2DDX12&) = delete;
    Texture2DDX12& operator=(const Texture2DDX12&) = delete;

    Texture2DDX12(Texture2DDX12&&) noexcept = default;
    Texture2DDX12& operator=(Texture2DDX12&&) noexcept = default;

    void setDebugName(const std::wstring& name);

    void loadFromFile(
        std::string_view path, 
        const bool needToFlip = false
    );

    bool valid() const { return image_.valid(); }

    ImageDX12& image() { return image_; }
    const ImageDX12& image() const { return image_; }

    ID3D12Resource* resource() const { return image_.resource(); }

    uint32_t width() const { return image_.width(); }
    uint32_t height() const { return image_.height(); }
    DXGI_FORMAT format() const { return image_.format(); }

private:
    void destroy();
private:
    DX12Main* dx_{ nullptr };
    ImageDX12 image_;
};

#endif
