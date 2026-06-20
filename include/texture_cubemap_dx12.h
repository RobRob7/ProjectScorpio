#ifndef TEXTURE_CUBEMAP_DX12_H
#define TEXTURE_CUBEMAP_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "image_dx12.h"

#include <string_view>
#include <cstdint>
#include <string>
#include <array>

class DX12Main;

class TextureCubemapDX12
{
public:
    explicit TextureCubemapDX12(DX12Main& dx);
    ~TextureCubemapDX12();

    TextureCubemapDX12(const TextureCubemapDX12&) = delete;
    TextureCubemapDX12& operator=(const TextureCubemapDX12&) = delete;

    TextureCubemapDX12(TextureCubemapDX12&&) noexcept = default;
    TextureCubemapDX12& operator=(TextureCubemapDX12&&) noexcept = default;

    void setDebugName(const std::wstring& name);

    void loadFromFiles(
        const std::array<std::string_view, 6>& faces, 
        const bool needToFlip = false
    );

    bool valid() const { return image_.valid(); }

    ID3D12Resource* resource() const { return image_.resource(); }

    uint32_t width() const { return image_.width(); }
    uint32_t height() const { return image_.height(); }
    DXGI_FORMAT format() const { return image_.format(); }

    ImageDX12& image() { return image_; }
    const ImageDX12& image() const { return image_; }

private:
    void destroy();
private:
    DX12Main* dx_{ nullptr };
    ImageDX12 image_;
};

#endif
