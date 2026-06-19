#include "dx12_main.h"

#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "frame_context_dx12.h"

#include <stdexcept>
#include <algorithm>

//--- HELPER ---//
static void ThrowIfFailed(HRESULT hr, const char* message)
{
    if (FAILED(hr))
    {
        throw std::runtime_error(message);
    }
} // end of ThrowIfFailed()


//--- PUBLIC ---//
DX12Main::DX12Main(GLFWwindow* window)
    : window_(window)
{
} // end of constructor

DX12Main::~DX12Main()
{
    if (initialized_)
    {
        waitIdle();
        flushAllRetiredResources();
        cleanupSwapChain();
    }

    if (frameFenceEvent_)
    {
        CloseHandle(frameFenceEvent_);
        frameFenceEvent_ = nullptr;
    }
} // end of destructor

void DX12Main::setDebugName(ID3D12Object* object, const wchar_t* name) const
{
    if (object)
    {
        object->SetName(name);
    }
} // end of setDebugName()

void DX12Main::setDebugName(IDXGIObject* object, const char* name) const
{
    if (object && name)
    {
        object->SetPrivateData(
            WKPDID_D3DDebugObjectName,
            static_cast<uint32_t>(std::strlen(name)),
            name
        );
    }
} // end of setDebugName()

void DX12Main::init()
{
    enableDebugLayer();
    createFactory();
    checkTearingSupport();
    pickAdapter();
    createDevice();
    checkFeatureSupport();

    createCommandQueue();
    createSwapChain();
    createDescriptorHeaps();
    createRenderTargetViews();
    createDepthResources();

    createCommandAllocators();
    createCommandLists();
    createSyncObjects();

    initialized_ = true;
} // end of init()

void DX12Main::waitIdle()
{
    if (!graphicsQueue_ || !frameFence_)
    {
        return;
    }

    const uint64_t fenceValue = nextFenceValue_;

    ThrowIfFailed(
        graphicsQueue_->Signal(frameFence_.Get(), fenceValue),
        "failed to signal waitIdle fence."
    );

    if (frameFence_->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(
            frameFence_->SetEventOnCompletion(fenceValue, frameFenceEvent_),
            "failed to set waitIdle fence event."
        );

        WaitForSingleObject(frameFenceEvent_, INFINITE);
    }
} // end of waitIdle()

bool DX12Main::beginFrame(FrameContextDX12& out)
{
    if (framebufferResized_)
    {
        recreateSwapChain();
        return false;
    }

    const uint64_t fenceValue = frameFenceValues_[currentFrame_];

    if (fenceValue != 0 && frameFence_->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(
            frameFence_->SetEventOnCompletion(fenceValue, frameFenceEvent_),
            "failed to set fence event."
        );

        WaitForSingleObject(frameFenceEvent_, INFINITE);
    }

    flushRetiredResources(currentFrame_);
    processPendingUploads();

    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

    ThrowIfFailed(
        commandAllocators_[currentFrame_]->Reset(),
        "failed to reset command allocator."
    );

    ThrowIfFailed(
        commandList_->Reset(commandAllocators_[currentFrame_].Get(), nullptr),
        "failed to reset command list."
    );

    if (swapChainStates_[currentBackBufferIndex_] != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        //D3D12_RESOURCE_BARRIER barrier =

        //commandList_->ResourceBarrier(1, &barrier);
        swapChainStates_[currentBackBufferIndex_] = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // fill in frame contents
    out.cmd = commandList_.Get();

    out.width = swapChainWidth_;
    out.height = swapChainHeight_;

    out.frameIndex = currentFrame_;
    out.imageIndex = currentBackBufferIndex_;

    out.colorImage = swapChainBuffers_[currentBackBufferIndex_].Get();
    out.colorRTV = getCurrentRTV();
    out.colorState = &swapChainStates_[currentBackBufferIndex_];

    out.depthImage = depthImage_.Get();
    out.depthDSV = getDSV();
    out.depthState = &depthState_;

    return true;
} // end of beginFrame()

bool DX12Main::endFrame(const FrameContextDX12& frame)
{
    if (swapChainStates_[currentBackBufferIndex_] != D3D12_RESOURCE_STATE_PRESENT)
    {
        //D3D12_RESOURCE_BARRIER barrier = 

        //commandList_->ResourceBarrier(1, &barrier);
        swapChainStates_[currentBackBufferIndex_] = D3D12_RESOURCE_STATE_PRESENT;
    }

    ThrowIfFailed(
        commandList_->Close(),
        "failed to close command list."
    );

    ID3D12CommandList* commandLists[] =
    {
        commandList_.Get()
    };

    graphicsQueue_->ExecuteCommandLists(1, commandLists);

    const uint32_t syncInterval = vsyncEnabled_ ? 1 : 0;
    const uint32_t presentFlags =
        (!vsyncEnabled_ && tearingSupported_) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    HRESULT presentResult = swapChain_->Present(syncInterval, presentFlags);

    if (presentResult == DXGI_ERROR_DEVICE_REMOVED ||
        presentResult == DXGI_ERROR_DEVICE_RESET)
    {
        return false;
    }

    if (FAILED(presentResult))
    {
        throw std::runtime_error("failed to present swapchain.");
    }

    const uint64_t fenceValue = nextFenceValue_++;

    ThrowIfFailed(
        graphicsQueue_->Signal(frameFence_.Get(), fenceValue),
        "failed to signal frame fence."
    );

    frameFenceValues_[currentFrame_] = fenceValue;

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;

    return true;
} // end of endFrame()

void DX12Main::submitUpload(
    ComPtr<ID3D12CommandAllocator>&& allocator,
    ComPtr<ID3D12GraphicsCommandList4>&& cmd,
    std::vector<BufferVk>&& uploadBuffers
)
{
    ThrowIfFailed(
        cmd->Close(),
        "failed to close upload command list."
    );

    ID3D12CommandList* commandLists[] =
    {
        commandList_.Get()
    };

    graphicsQueue_->ExecuteCommandLists(1, commandLists);

    const uint64_t fenceValue = nextFenceValue_++;

    ThrowIfFailed(
        graphicsQueue_->Signal(frameFence_.Get(), fenceValue),
        "failed to signal upload fence."
    );

    pendingUploads_.push_back(PendingUpload{
        std::move(allocator),
        std::move(cmd),
        fenceValue,
        std::move(uploadBuffers)
    });
} // end of submitUpload()

void DX12Main::processPendingUploads()
{
    const uint64_t completedValue = frameFence_->GetCompletedValue();

    auto it = pendingUploads_.begin();

    while (it != pendingUploads_.end())
    {
        if (completedValue >= it->fenceValue)
        {
            it = pendingUploads_.erase(it);
        }
        else
        {
            ++it;
        }
    } // end while
} // end of processPendingUploads()


//--- PRIVATE ---//

void DX12Main::enableDebugLayer()
{
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif
} // end of enableDebugLayer()

void DX12Main::createFactory()
{
    uint32_t flags = 0;

#ifdef _DEBUG
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(
        CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_)),
        "failed to create DXGI factory."
    );
} // end of createFactory()

void DX12Main::pickAdapter()
{
    ComPtr<IDXGIAdapter1> candidate;

    for (uint32_t i = 0;
         factory_->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&candidate)
         ) != DXGI_ERROR_NOT_FOUND;
        ++i)
    {
        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(
            candidate.Get(),
            D3D_FEATURE_LEVEL_12_2,
            __uuidof(ID3D12Device),
            nullptr)))
        {
            ThrowIfFailed(
                candidate.As(&adapter_),
                "failed to cast adapter to IDXGIAdapter4."
            );

            return;
        }
    }

    throw std::runtime_error("failed to find a suitable DX12 adapter.");
} // end of pickAdapter()

void DX12Main::createDevice()
{
    ThrowIfFailed(
        D3D12CreateDevice(
            adapter_.Get(),
            D3D_FEATURE_LEVEL_12_2,
            IID_PPV_ARGS(&device_)
        ),
        "failed to create D3D12 device."
    );

    setDebugName(device_.Get(), L"DX12 Device");
} // end of createDevice()

void DX12Main::checkFeatureSupport()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};

    if (SUCCEEDED(device_->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS5,
        &options,
        sizeof(options))))
    {
        supportsRayTracing_ =
            options.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    }
    else
    {
        supportsRayTracing_ = false;
    }
} // end of checkFeatureSupport()

void DX12Main::checkTearingSupport()
{
    bool allowTearing = false;

    if (factory_)
    {
        HRESULT hr = factory_->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &allowTearing,
            sizeof(allowTearing)
        );

        tearingSupported_ = SUCCEEDED(hr) && allowTearing;
    }
} // end of checkTearingSupport()

void DX12Main::createCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC desc{
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0
    };

        ThrowIfFailed(
            device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&graphicsQueue_)),
            "failed to create graphics command queue."
        );

    setDebugName(graphicsQueue_.Get(), L"Graphics Command Queue");
} // end of createCommandQueue()

void DX12Main::createSwapChain()
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);

    swapChainWidth_ = static_cast<uint32_t>(std::max(width, 1));
    swapChainHeight_ = static_cast<uint32_t>(std::max(height, 1));

    HWND hwnd = glfwGetWin32Window(window_);

    DXGI_SWAP_CHAIN_DESC1 desc{
        .Width = swapChainWidth_,
        .Height = swapChainHeight_,
        .Format = swapChainFormat_,
        .Stereo = FALSE,
        .SampleDesc{
            .Count = 1,
            .Quality = 0
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = MAX_FRAMES_IN_FLIGHT,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U
    };

    ComPtr<IDXGISwapChain1> swapChain;

    ThrowIfFailed(
        factory_->CreateSwapChainForHwnd(
            graphicsQueue_.Get(),
            hwnd,
            &desc,
            nullptr,
            nullptr,
            &swapChain
        ),
        "failed to create swapchain."
    );

    ThrowIfFailed(
        factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER),
        "failed to disable DXGI alt+enter."
    );

    ThrowIfFailed(
        swapChain.As(&swapChain_),
        "failed to cast swapchain to IDXGISwapChain3."
    );

    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

    setDebugName(swapChain_.Get(), "Main Swapchain");
} // end of createSwapChain()

void DX12Main::createDescriptorHeaps()
{
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = MAX_FRAMES_IN_FLIGHT,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0
        };

        ThrowIfFailed(
            device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap_)),
            "failed to create RTV descriptor heap."
        );

        rtvDescriptorSize_ =
            device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0
        };

        ThrowIfFailed(
            device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvHeap_)),
            "failed to create DSV descriptor heap."
        );

        dsvDescriptorSize_ =
            device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 4096,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0
        };

        ThrowIfFailed(
            device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvUavCbvHeap_)),
            "failed to create CBV/SRV/UAV descriptor heap."
        );

        srvUavCbvDescriptorSize_ =
            device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
} // end of createDescriptorHeaps()

void DX12Main::createRenderTargetViews()
{
    swapChainBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    swapChainStates_.resize(MAX_FRAMES_IN_FLIGHT);

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        ThrowIfFailed(
            swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffers_[i])),
            "failed to get swapchain back buffer."
        );

        device_->CreateRenderTargetView(
            swapChainBuffers_[i].Get(),
            nullptr,
            handle
        );

        swapChainStates_[i] = D3D12_RESOURCE_STATE_PRESENT;

        handle.ptr += rtvDescriptorSize_;
    } // end for
} // end of createRenderTargetViews()

void DX12Main::createDepthResources()
{
    D3D12_HEAP_PROPERTIES heapProps{
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    D3D12_RESOURCE_DESC desc{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = swapChainWidth_,
        .Height = swapChainHeight_,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = depthFormat_,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0
        },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };

    D3D12_CLEAR_VALUE clearValue{
        .Format = depthFormat_,
        .DepthStencil = {
            .Depth = 1.0f,
            .Stencil = 0
        }
    };

    ThrowIfFailed(
        device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&depthImage_)
        ),
        "failed to create depth image."
    );

    device_->CreateDepthStencilView(
        depthImage_.Get(),
        nullptr,
        dsvHeap_->GetCPUDescriptorHandleForHeapStart()
    );

    depthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    setDebugName(depthImage_.Get(), L"Depth Image");
} // end of createDepthResources()

void DX12Main::createCommandAllocators()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        ThrowIfFailed(
            device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&commandAllocators_[i])
            ),
            "failed to create command allocator."
        );
    } // end for
} // end of createCommandAllocators()

void DX12Main::createCommandLists()
{
    ThrowIfFailed(
        device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocators_[0].Get(),
            nullptr,
            IID_PPV_ARGS(&commandList_)
        ),
        "failed to create command list."
    );

    ThrowIfFailed(
        commandList_->Close(),
        "failed to close initial command list."
    );
} // end of createCommandLists()

void DX12Main::createSyncObjects()
{
    ThrowIfFailed(
        device_->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&frameFence_)
        ),
        "failed to create frame fence."
    );

    frameFenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!frameFenceEvent_)
    {
        throw std::runtime_error("failed to create fence event.");
    }

    frameFenceValues_.fill(0);
    nextFenceValue_ = 1;
} // end of createSyncObjects()

void DX12Main::cleanupSwapChain()
{
    depthImage_.Reset();

    for (auto& buffer : swapChainBuffers_)
    {
        buffer.Reset();
    } // end for

    swapChainBuffers_.clear();
    swapChainStates_.clear();

    swapChain_.Reset();
} // end of cleanupSwapChain()

void DX12Main::recreateSwapChain()
{
    waitIdle();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);

    while (width == 0 || height == 0)
    {
        glfwWaitEvents();
        glfwGetFramebufferSize(window_, &width, &height);
    } // end while

    cleanupSwapChain();

    createSwapChain();
    createRenderTargetViews();
    createDepthResources();

    framebufferResized_ = false;
} // end of recreateSwapChain()

void DX12Main::flushRetiredResources(uint32_t frameIndex)
{
    retired_[frameIndex].buffers.clear();
    retired_[frameIndex].images.clear();
    retired_[frameIndex].accelStructures.clear();
} // end of flushRetiredResources()

void DX12Main::flushAllRetiredResources()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        flushRetiredResources(i);
    } // end for

    pendingUploads_.clear();
} // end of flushAllRetiredResources()
