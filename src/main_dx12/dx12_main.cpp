#include "dx12_main.h"

#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "utils_dx12.h"
#include "frame_context_dx12.h"

#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <vector>

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

void DX12Main::dumpDebugMessages(const char* context)
{
    if (!infoQueue_)
    {
        return;
    }

    const UINT64 count = infoQueue_->GetNumStoredMessages();

    if (count == 0)
    {
        return;
    }

    if (context)
    {
        std::cerr << "\n[D3D12 Debug] " << context << "\n";
    }
    else
    {
        std::cerr << "\n[D3D12 Debug]\n";
    }

    for (UINT64 i = 0; i < count; ++i)
    {
        SIZE_T messageLength = 0;
        HRESULT hr = infoQueue_->GetMessage(i, nullptr, &messageLength);

        if (FAILED(hr) || messageLength == 0)
        {
            continue;
        }

        std::vector<char> storage(messageLength);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());

        hr = infoQueue_->GetMessage(i, message, &messageLength);

        if (FAILED(hr))
        {
            continue;
        }

        std::cerr
            << "[D3D12] "
            << "Severity=" << static_cast<int>(message->Severity)
            << " Category=" << static_cast<int>(message->Category)
            << " ID=" << message->ID
            << "\n"
            << message->pDescription
            << "\n\n";
    } // end for

    infoQueue_->ClearStoredMessages();
} // end of dumpDebugMessages()

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

    DX12Utils::ThrowIfFailed(
        graphicsQueue_->Signal(frameFence_.Get(), fenceValue),
        "DX12Main::waitIdle - failed to signal waitIdle fence"
    );

    if (frameFence_->GetCompletedValue() < fenceValue)
    {
        DX12Utils::ThrowIfFailed(
            frameFence_->SetEventOnCompletion(fenceValue, frameFenceEvent_),
            "DX12Main::waitIdle - failed to set waitIdle fence event"
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
        DX12Utils::ThrowIfFailed(
            frameFence_->SetEventOnCompletion(fenceValue, frameFenceEvent_),
            "DX12Main::beginFrame - failed to set fence event"
        );

        WaitForSingleObject(frameFenceEvent_, INFINITE);
    }

    flushRetiredResources(currentFrame_);
    processPendingUploads();

    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

    DX12Utils::ThrowIfFailed(
        commandAllocators_[currentFrame_]->Reset(),
        "DX12Main::beginFrame - failed to reset command allocator"
    );

    DX12Utils::ThrowIfFailed(
        commandList_->Reset(commandAllocators_[currentFrame_].Get(), nullptr),
        "DX12Main::beginFrame - failed to reset command list"
    );

    if (swapChainStates_[currentBackBufferIndex_] != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        DX12Utils::TransitionResource(
            commandList_.Get(),
            swapChainBuffers_[currentBackBufferIndex_].Get(),
            swapChainStates_[currentBackBufferIndex_],
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
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
        DX12Utils::TransitionResource(
            commandList_.Get(),
            swapChainBuffers_[currentBackBufferIndex_].Get(),
            swapChainStates_[currentBackBufferIndex_],
            D3D12_RESOURCE_STATE_PRESENT
        );
    }

    DX12Utils::ThrowIfFailed(
        commandList_->Close(),
        "DX12Main::endFrame - failed to close command list"
    );

#ifdef _DEBUG
    dumpDebugMessages();
#endif

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
        throw std::runtime_error("DX12Main::endFrame - failed to present swapchain");
    }

    const uint64_t fenceValue = nextFenceValue_++;

    DX12Utils::ThrowIfFailed(
        graphicsQueue_->Signal(frameFence_.Get(), fenceValue),
        "DX12Main::endFrame - failed to signal frame fence"
    );

    frameFenceValues_[currentFrame_] = fenceValue;

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;

    return true;
} // end of endFrame()

void DX12Main::submitUpload(
    ComPtr<ID3D12CommandAllocator>&& allocator,
    ComPtr<ID3D12GraphicsCommandList4>&& cmd,
    std::vector<BufferDX12>&& uploadBuffers
)
{
    DX12Utils::ThrowIfFailed(
        cmd->Close(),
        "DX12Main::submitUpload - failed to close upload command list"
    );

    ID3D12CommandList* commandLists[] =
    {
        cmd.Get()
    };

    graphicsQueue_->ExecuteCommandLists(1, commandLists);

    const uint64_t fenceValue = nextFenceValue_++;

    DX12Utils::ThrowIfFailed(
        graphicsQueue_->Signal(frameFence_.Get(), fenceValue),
        "DX12Main::submitUpload - failed to signal upload fence"
    );

    pendingUploads_.push_back(PendingUploadDX12{
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

void DX12Main::allocateImGuiDescriptor(
    D3D12_CPU_DESCRIPTOR_HANDLE& outCpu,
    D3D12_GPU_DESCRIPTOR_HANDLE& outGpu
)
{
    for (uint32_t i = 0; i < IMGUI_DESCRIPTOR_COUNT; ++i)
    {
        if (!imguiDescriptorUsed_[i])
        {
            imguiDescriptorUsed_[i] = true;

            const uint32_t descriptorIndex = IMGUI_DESCRIPTOR_START + i;

            outCpu = srvUavCbvHeap_->GetCPUDescriptorHandleForHeapStart();
            outGpu = srvUavCbvHeap_->GetGPUDescriptorHandleForHeapStart();

            outCpu.ptr += descriptorIndex * srvUavCbvDescriptorSize_;
            outGpu.ptr += descriptorIndex * srvUavCbvDescriptorSize_;

            return;
        }
    } // end for

    throw std::runtime_error("DX12Main::allocateImGuiDescriptor - no free ImGui descriptors");
} // end of allocateImGuiDescriptor()

void DX12Main::freeImGuiDescriptor(
    D3D12_CPU_DESCRIPTOR_HANDLE cpu,
    D3D12_GPU_DESCRIPTOR_HANDLE gpu
)
{
    const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
        srvUavCbvHeap_->GetCPUDescriptorHandleForHeapStart();

    const uint32_t descriptorIndex =
        static_cast<uint32_t>((cpu.ptr - heapStart.ptr) / srvUavCbvDescriptorSize_);

    if (descriptorIndex < IMGUI_DESCRIPTOR_START ||
        descriptorIndex >= IMGUI_DESCRIPTOR_START + IMGUI_DESCRIPTOR_COUNT)
    {
        return;
    }

    const uint32_t localIndex = descriptorIndex - IMGUI_DESCRIPTOR_START;
    imguiDescriptorUsed_[localIndex] = false;
} // end of freeImGuiDescriptor()

std::string DX12Main::getAdapterName() const
{
    if (!adapter_)
    {
        return "Unknown DX12 adapter";
    }

    DXGI_ADAPTER_DESC3 desc{};
    adapter_->GetDesc3(&desc);

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        desc.Description,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    std::string result(size, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        desc.Description,
        -1,
        result.data(),
        size,
        nullptr,
        nullptr
    );

    if (!result.empty() && result.back() == '\0')
    {
        result.pop_back();
    }

    return result;
} // end of getAdapterName()


//--- PRIVATE ---//
void DX12Main::setupDebugInfoQueue()
{
    if (SUCCEEDED(device_.As(&infoQueue_)))
    {
        infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, FALSE);
        infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
        infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

        infoQueue_->SetMessageCountLimit(1024);
    }
} // end of setupDebugInfoQueue()

void DX12Main::enableDebugLayer()
{
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1)))
        {
            debugController1->SetEnableGPUBasedValidation(TRUE);
        }
    }
#endif
} // end of enableDebugLayer()

void DX12Main::createFactory()
{
    uint32_t flags = 0;

#ifdef _DEBUG
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    DX12Utils::ThrowIfFailed(
        CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_)),
        "DX12Main::createFactory - failed to create DXGI factory"
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
            DX12Utils::ThrowIfFailed(
                candidate.As(&adapter_),
                "DX12Main::pickAdapter - failed to cast adapter to IDXGIAdapter4"
            );

            return;
        }
    }

    throw std::runtime_error("fDX12Main::pickAdapter - failed to find a suitable DX12 adapter");
} // end of pickAdapter()

void DX12Main::createDevice()
{
    DX12Utils::ThrowIfFailed(
        D3D12CreateDevice(
            adapter_.Get(),
            D3D_FEATURE_LEVEL_12_2,
            IID_PPV_ARGS(&device_)
        ),
        "DX12Main::createDevice - failed to create D3D12 device"
    );

#ifdef _DEBUG
    setupDebugInfoQueue();
#endif

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
    BOOL allowTearing = FALSE;

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

        DX12Utils::ThrowIfFailed(
            device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&graphicsQueue_)),
            "DX12Main::createCommandQueue - failed to create graphics command queue"
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

    DX12Utils::ThrowIfFailed(
        factory_->CreateSwapChainForHwnd(
            graphicsQueue_.Get(),
            hwnd,
            &desc,
            nullptr,
            nullptr,
            &swapChain
        ),
        "DX12Main::createSwapChain - failed to create swapchain"
    );

    DX12Utils::ThrowIfFailed(
        factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER),
        "DX12Main::createSwapChain - failed to disable DXGI alt+enter"
    );

    DX12Utils::ThrowIfFailed(
        swapChain.As(&swapChain_),
        "DX12Main::createSwapChain - failed to cast swapchain to IDXGISwapChain3"
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

        DX12Utils::ThrowIfFailed(
            device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap_)),
            "DX12Main::createDescriptorHeaps - failed to create RTV descriptor heap"
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

        DX12Utils::ThrowIfFailed(
            device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvHeap_)),
            "DX12Main::createDescriptorHeaps - failed to create DSV descriptor heap"
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

        DX12Utils::ThrowIfFailed(
            device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvUavCbvHeap_)),
            "DX12Main::createDescriptorHeaps - failed to create CBV/SRV/UAV descriptor heap"
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
        DX12Utils::ThrowIfFailed(
            swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffers_[i])),
            "DX12Main::createRenderTargetViews - failed to get swapchain back buffer"
        );

        setDebugName(
            swapChainBuffers_[i].Get(),
            (L"Swapchain Backbuffer " + std::to_wstring(i)).c_str()
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

    DX12Utils::ThrowIfFailed(
        device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&depthImage_)
        ),
        "DX12Main::createDepthResources - failed to create depth image"
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
        DX12Utils::ThrowIfFailed(
            device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&commandAllocators_[i])
            ),
            "DX12Main::createCommandAllocators - failed to create command allocator"
        );
    } // end for
} // end of createCommandAllocators()

void DX12Main::createCommandLists()
{
    DX12Utils::ThrowIfFailed(
        device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocators_[0].Get(),
            nullptr,
            IID_PPV_ARGS(&commandList_)
        ),
        "DX12Main::createCommandLists - failed to create command list"
    );

    DX12Utils::ThrowIfFailed(
        commandList_->Close(),
        "DX12Main::createCommandLists - failed to close initial command list"
    );
} // end of createCommandLists()

void DX12Main::createSyncObjects()
{
    DX12Utils::ThrowIfFailed(
        device_->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&frameFence_)
        ),
        "DX12Main::createSyncObjects - failed to create frame fence."
    );

    frameFenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!frameFenceEvent_)
    {
        throw std::runtime_error("DX12Main::createSyncObjects - failed to create fence event");
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
    //retired_[frameIndex].accelStructures.clear();
} // end of flushRetiredResources()

void DX12Main::flushAllRetiredResources()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        flushRetiredResources(i);
    } // end for

    pendingUploads_.clear();
} // end of flushAllRetiredResources()
