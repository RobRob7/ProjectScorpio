#ifndef DX12_MAIN_H
#define DX12_MAIN_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <d3d12sdklayers.h>

#include "image_dx12.h"
#include "buffer_dx12.h"
//#include "acceleration_structure_vk.h"

#include <vector>
#include <cstdint>
#include <array>
#include <utility>
#include <string>

struct GLFWwindow;
struct FrameContextDX12;

using Microsoft::WRL::ComPtr;

struct Extent2D
{
    uint32_t width;
    uint32_t height;
};

struct PendingUploadDX12
{
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList4> cmd;
    uint64_t fenceValue{};
    std::vector<BufferDX12> uploadBuffers;
};

struct RetiredFrameResourcesDX12
{
    std::vector<BufferDX12> buffers;
    std::vector<ImageDX12> images;
    //std::vector<AccelerationStructureVk> accelStructures;
};

class DX12Main
{
public:
    explicit DX12Main(GLFWwindow* window);
    ~DX12Main();

    void dumpDebugMessages(const char* context = nullptr);

    void setDebugName(ID3D12Object* object, const wchar_t* name) const;
    void setDebugName(IDXGIObject* object, const char* name) const;

    void init();
    void waitIdle();

    bool beginFrame(FrameContextDX12& out);
    bool endFrame(const FrameContextDX12& frame);

    void submitUpload(
        ComPtr<ID3D12CommandAllocator>&& allocator,
        ComPtr<ID3D12GraphicsCommandList4>&& cmd,
        std::vector<BufferDX12>&& uploadBuffers
    );

    void processPendingUploads();

    void allocateImGuiDescriptor(
        D3D12_CPU_DESCRIPTOR_HANDLE& outCpu,
        D3D12_GPU_DESCRIPTOR_HANDLE& outGpu
    );

    void freeImGuiDescriptor(
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu
    );

    std::string getAdapterName() const;

    void notifyFramebufferResized() { framebufferResized_ = true; }

    DXGI_FORMAT getDepthFormat() const { return depthFormat_; }
    DXGI_FORMAT getSwapChainImageFormat() const { return swapChainFormat_; }
    Extent2D getSwapChainExtent() const { return Extent2D{ swapChainWidth_, swapChainHeight_ }; }

    ID3D12Device5* getDevice() const { return device_.Get(); }
    ID3D12CommandQueue* getGraphicsQueue() const { return graphicsQueue_.Get(); }
    IDXGISwapChain3* getSwapChain() const { return swapChain_.Get(); }

    ID3D12Resource* getCurrentBackBuffer() const { return swapChainBuffers_[currentBackBufferIndex_].Get(); }
    ID3D12Resource* getBackBuffer(uint32_t index) const { return swapChainBuffers_[index].Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRTV() const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            rtvHeap_->GetCPUDescriptorHandleForHeapStart();

        handle.ptr += currentBackBufferIndex_ * rtvDescriptorSize_;

        return handle;
    } // end of getCurrentRTV()

    D3D12_CPU_DESCRIPTOR_HANDLE getDSV() const { return dsvHeap_->GetCPUDescriptorHandleForHeapStart(); }

    ID3D12DescriptorHeap* getImguiSrvHeap() const { return srvUavCbvHeap_.Get(); }

    uint32_t getSwapChainImageCount() const { return static_cast<uint32_t>(swapChainBuffers_.size()); }

    uint32_t getMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }
    uint32_t getPrevFrameIndex() const { return (currentFrame_ + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT; }
    uint32_t currentFrameIndex() const { return currentFrame_; }
    uint32_t currentBackBufferIndex() const { return currentBackBufferIndex_; }

    bool getVSync() const { return vsyncEnabled_; }
    void setVSync(bool enabled) { vsyncEnabled_ = enabled; }

    bool supportsRayTracing() const { return supportsRayTracing_; }
    bool getRTStatus() const { return rtEnabled_; }
    void setRTStatus(bool enable) { rtEnabled_ = enable; }

    void retireBuffer(uint32_t frameIndex, BufferDX12&& buffer)
    {
        if (buffer.valid())
        {
            retired_[frameIndex].buffers.push_back(std::move(buffer));
        }
    } // end of retireBuffer()
    void retireImage(uint32_t frameIndex, ImageDX12&& image)
    {
        if (image.valid())
        {
            retired_[frameIndex].images.push_back(std::move(image));
        }
    } // end of retireImage()
    //void retireAccelerationStructure(uint32_t frameIndex, AccelerationStructureVk&& as)
    //{
    //    if (as.valid())
    //    {
    //        retired_[frameIndex].accelStructures.push_back(std::move(as));
    //    }
    //} // end of retireAccelerationStructure()

private:
    void setupDebugInfoQueue();
    void enableDebugLayer();
    void createFactory();
    void pickAdapter();
    void createDevice();
    void checkFeatureSupport();
    void checkTearingSupport();
    void createCommandQueue();
    void createSwapChain();
    void createDescriptorHeaps();
    void createRenderTargetViews();
    void createDepthResources();
    void createCommandAllocators();
    void createCommandLists();
    void createSyncObjects();

    void cleanupSwapChain();
    void recreateSwapChain();

    void flushRetiredResources(uint32_t frameIndex);
    void flushAllRetiredResources();

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT{ 3 };

    GLFWwindow* window_{};

    bool initialized_{ false };
    bool framebufferResized_{ false };
    bool vsyncEnabled_{ true };
    bool tearingSupported_{ false };
    bool supportsRayTracing_{ false };

    bool rtEnabled_{ false };

    uint32_t currentFrame_{ 0 };
    uint32_t currentBackBufferIndex_{ 0 };

    ComPtr<IDXGIFactory7> factory_;
    ComPtr<IDXGIAdapter4> adapter_;
    ComPtr<ID3D12Device5> device_;

    ComPtr<ID3D12CommandQueue> graphicsQueue_;
    ComPtr<IDXGISwapChain3> swapChain_;

    std::vector<ComPtr<ID3D12Resource>> swapChainBuffers_;
    std::vector<D3D12_RESOURCE_STATES> swapChainStates_;
    DXGI_FORMAT swapChainFormat_{ DXGI_FORMAT_R8G8B8A8_UNORM };
    uint32_t swapChainWidth_{};
    uint32_t swapChainHeight_{};

    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    ComPtr<ID3D12DescriptorHeap> srvUavCbvHeap_;

    UINT rtvDescriptorSize_{};
    UINT dsvDescriptorSize_{};
    UINT srvUavCbvDescriptorSize_{};

    ComPtr<ID3D12Resource> depthImage_;
    D3D12_RESOURCE_STATES depthState_{ D3D12_RESOURCE_STATE_DEPTH_WRITE };
    DXGI_FORMAT depthFormat_{ DXGI_FORMAT_D32_FLOAT };

    std::array<ComPtr<ID3D12CommandAllocator>, MAX_FRAMES_IN_FLIGHT> commandAllocators_;
    ComPtr<ID3D12GraphicsCommandList4> commandList_;

    ComPtr<ID3D12Fence> frameFence_;
    std::array<uint64_t, MAX_FRAMES_IN_FLIGHT> frameFenceValues_{};
    uint64_t nextFenceValue_{ 1 };
    HANDLE frameFenceEvent_{ nullptr };

    std::vector<PendingUploadDX12> pendingUploads_;
    std::array<RetiredFrameResourcesDX12, MAX_FRAMES_IN_FLIGHT> retired_;

    static constexpr uint32_t IMGUI_DESCRIPTOR_START = 0;
    static constexpr uint32_t IMGUI_DESCRIPTOR_COUNT = 64;

    std::array<bool, IMGUI_DESCRIPTOR_COUNT> imguiDescriptorUsed_{};

    ComPtr<ID3D12InfoQueue> infoQueue_;
};

#endif