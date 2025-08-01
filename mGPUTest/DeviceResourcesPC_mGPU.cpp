//
// DeviceResources.cpp - A wrapper for the Direct3D 12 device and swapchain (mGPU variant)
//

#include "pch.h"
#include "DeviceResourcesPC_mGPU.h"

#define ENABLE_GPU_VALIDATION

using namespace DirectX;
using namespace DX;

using Microsoft::WRL::ComPtr;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

namespace
{
    inline DXGI_FORMAT NoSRGB(DXGI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8X8_UNORM;
        default:                                return fmt;
        }
    }

    inline long ComputeIntersectionArea(
        long ax1, long ay1, long ax2, long ay2,
        long bx1, long by1, long bx2, long by2) noexcept
    {
        return std::max(0l, std::min(ax2, bx2) - std::max(ax1, bx1)) * std::max(0l, std::min(ay2, by2) - std::max(ay1, by1));
    }
}

bool DeviceResources::s_debugForceWarp = false;
bool DeviceResources::s_debugPreferMinPower = false;
int DeviceResources::s_debugAdapterOrdinal = -1;

// Constructor for DeviceResources.
DeviceResources::DeviceResources(
    DXGI_FORMAT backBufferFormat,
    DXGI_FORMAT depthBufferFormat,
    UINT backBufferCount,
    D3D_FEATURE_LEVEL minFeatureLevel,
    unsigned int flags,
    unsigned int deviceCount) noexcept(false) :
        m_backBufferIndex(0),
        m_deviceCount(deviceCount),
        m_screenViewport{},
        m_scissorRect{},
        m_backBufferFormat(backBufferFormat),
        m_depthBufferFormat(depthBufferFormat),
        m_backBufferCount(backBufferCount),
        m_d3dMinFeatureLevel(minFeatureLevel),
        m_clearColor{},
        m_window(nullptr),
        m_d3dFeatureLevel(D3D_FEATURE_LEVEL_11_0),
        m_dxgiFactoryFlags(0),
        m_outputSize{0, 0, 1, 1},
        m_colorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),
        m_options(flags),
        m_pAdaptersD3D(nullptr)
{
    if (backBufferCount < 2 || backBufferCount > MAX_BACK_BUFFER_COUNT)
    {
        throw std::out_of_range("invalid backBufferCount");
    }

    if (minFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        throw std::out_of_range("minFeatureLevel too low");
    }

    m_pAdaptersD3D = new PerAdapter[deviceCount];
    assert(m_pAdaptersD3D != nullptr);
}

// Destructor for DeviceResources.
DeviceResources::~DeviceResources()
{
    // Ensure that the GPU is no longer referencing resources that are about to be destroyed.
    WaitForGpu();
    if(m_pAdaptersD3D != nullptr)
    {
        delete[] m_pAdaptersD3D;
    }
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DeviceResources::CreateDeviceResources()
{
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    //
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();
        #ifdef ENABLE_GPU_VALIDATION
            ComPtr<ID3D12Debug1> debugController1;
            if (SUCCEEDED(debugController.As(&debugController1)))
            {
                debugController1->SetEnableGPUBasedValidation(true);
            }
        #endif
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

    #ifndef __MINGW32__
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            m_dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        }
    #endif
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));

    // Determines whether tearing support is available for fullscreen borderless windows.
    if (m_options & c_AllowTearing)
    {
        BOOL allowTearing = FALSE;

        ComPtr<IDXGIFactory5> factory5;
        HRESULT hr = m_dxgiFactory.As(&factory5);
        if (SUCCEEDED(hr))
        {
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }

        if (FAILED(hr) || !allowTearing)
        {
            m_options &= ~c_AllowTearing;
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
    }

    IDXGIAdapter1** ppAdapter = new IDXGIAdapter1*[m_deviceCount];
    assert(ppAdapter != nullptr);
    GetAdapter(ppAdapter, m_deviceCount);

    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        // Create the DX12 API device object.
        ThrowIfFailed(D3D12CreateDevice(ppAdapter[adapterIdx], m_d3dMinFeatureLevel, IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_d3dDevice.ReleaseAndGetAddressOf())));
        m_pAdaptersD3D[adapterIdx].m_d3dDevice->SetName(L"DeviceResources");

#ifndef NDEBUG
        // Configure debug device (if active).
        ComPtr<ID3D12InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(m_pAdaptersD3D[adapterIdx].m_d3dDevice.As(&d3dInfoQueue)))
        {
#ifdef _DEBUG
            d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
            D3D12_MESSAGE_ID hide[] =
            {
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
                // Workarounds for debug layer issues on hybrid-graphics systems
                D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
                D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
            };
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
#endif

        // Determine maximum supported feature level for this device
        static const D3D_FEATURE_LEVEL s_featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
        {
            static_cast<UINT>(std::size(s_featureLevels)), s_featureLevels, D3D_FEATURE_LEVEL_11_0
        };

        HRESULT hr = m_pAdaptersD3D[adapterIdx].m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
        if (SUCCEEDED(hr))
        {
            m_d3dFeatureLevel = featLevels.MaxSupportedFeatureLevel;
        }
        else
        {
            m_d3dFeatureLevel = m_d3dMinFeatureLevel;
        }

        // Create the command queue.
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_commandQueue.ReleaseAndGetAddressOf())));
        m_pAdaptersD3D[adapterIdx].m_commandQueue->SetName(L"DeviceResources");

        // Create descriptor heaps for render target views and depth stencil views.
        D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
        rtvDescriptorHeapDesc.NumDescriptors = m_backBufferCount;
        rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_rtvDescriptorHeap.ReleaseAndGetAddressOf())));

        m_pAdaptersD3D[adapterIdx].m_rtvDescriptorHeap->SetName(L"DeviceResources");

        m_pAdaptersD3D[adapterIdx].m_rtvDescriptorSize = m_pAdaptersD3D[adapterIdx].m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
            dsvDescriptorHeapDesc.NumDescriptors = 1;
            dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

            ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_dsvDescriptorHeap.ReleaseAndGetAddressOf())));

            m_pAdaptersD3D[adapterIdx].m_dsvDescriptorHeap->SetName(L"DeviceResources");
        }

        // Create a command allocator for each back buffer that will be rendered to.
        for (UINT n = 0; n < m_backBufferCount; n++)
        {
            ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_commandAllocators[n].ReleaseAndGetAddressOf())));

            wchar_t name[25] = {};
            swprintf_s(name, L"Render target %u", n);
            m_pAdaptersD3D[adapterIdx].m_commandAllocators[n]->SetName(name);
        }

        // Create a command list for recording graphics commands.
        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pAdaptersD3D[adapterIdx].m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_commandList.ReleaseAndGetAddressOf())));
        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_commandList->Close());

        m_pAdaptersD3D[adapterIdx].m_commandList->SetName(L"DeviceResources");

        // Create a fence for tracking GPU execution progress.
        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateFence(m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_fence.ReleaseAndGetAddressOf())));
        m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex]++;
        m_pAdaptersD3D[adapterIdx].m_fence->SetName(L"DeviceResources");

        m_pAdaptersD3D[adapterIdx].m_fenceEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
        if (!m_pAdaptersD3D[adapterIdx].m_fenceEvent.IsValid())
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEventEx");
        }
    }//End of # adapters loop

    delete [] ppAdapter;
    ppAdapter = nullptr;
}

// These resources need to be recreated every time the window size is changed.
void DeviceResources::CreateWindowSizeDependentResources()
{
    if (!m_window)
    {
        throw std::logic_error("Call SetWindow with a valid Win32 window handle");
    }

    // Wait until all previous GPU work is complete.
    WaitForGpu();

    // Release resources that are tied to the swap chain and update fence values.
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        for (UINT n = 0; n < m_backBufferCount; n++)
        {
            m_pAdaptersD3D[adapterIdx].m_renderTargets[n].Reset();
            m_pAdaptersD3D[adapterIdx].m_fenceValues[n] = m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex];
        }
    }

    // Determine the render target size in pixels.
    const UINT backBufferWidth = std::max<UINT>(static_cast<UINT>(m_outputSize.right - m_outputSize.left), 1u);
    const UINT backBufferHeight = std::max<UINT>(static_cast<UINT>(m_outputSize.bottom - m_outputSize.top), 1u);
    const DXGI_FORMAT backBufferFormat = NoSRGB(m_backBufferFormat);

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        // If the swap chain already exists, resize it.
        HRESULT hr = m_swapChain->ResizeBuffers(
            m_backBufferCount,
            backBufferWidth,
            backBufferHeight,
            backBufferFormat,
            (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
            );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_pAdaptersD3D[DT_Primary].m_d3dDevice->GetDeviceRemovedReason() : hr));
            OutputDebugStringA(buff);
#endif
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            HandleDeviceLost();

            // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
            // and correctly set up the new device.
            return;
        }
        else
        {
            ThrowIfFailed(hr);
        }
    }
    else
    {
        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = m_backBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        // Create a swap chain for the window.
        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(
            m_pAdaptersD3D[0].m_commandQueue.Get(),
            m_window,
            &swapChainDesc,
            &fsSwapChainDesc,
            nullptr,
            swapChain.GetAddressOf()
            ));

        ThrowIfFailed(swapChain.As(&m_swapChain));

        // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
        ThrowIfFailed(m_dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // Handle color space settings for HDR
    UpdateColorSpace();

    // Obtain the back buffers for this window which will be the final render targets
    // and create render target views for each of them.
    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(m_pAdaptersD3D[DT_Primary].m_renderTargets[n].GetAddressOf())));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", n);
        m_pAdaptersD3D[DT_Primary].m_renderTargets[n]->SetName(name);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_backBufferFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    #ifdef __MINGW32__
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        std::ignore = m_pAdaptersD3D[DT_Primary].m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(&cpuHandle);
    #else
        auto cpuHandle = m_pAdaptersD3D[DT_Primary].m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    #endif

        const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(cpuHandle, static_cast<INT>(n), m_pAdaptersD3D[DT_Primary].m_rtvDescriptorSize);
        m_pAdaptersD3D[DT_Primary].m_d3dDevice->CreateRenderTargetView(m_pAdaptersD3D[DT_Primary].m_renderTargets[n].Get(), &rtvDesc, rtvDescriptor);
    }

    // Second adapter if there is one
    for (unsigned int adapterIdx = 1; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        for (UINT n = 0; n < m_backBufferCount; n++)
        {
            // Create an intermediate render target and view on the secondary adapter.
            const CD3DX12_CLEAR_VALUE clearValue(m_backBufferFormat, m_clearColor);
            D3D12_RESOURCE_DESC renderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                m_backBufferFormat, backBufferWidth, backBufferHeight, 1, 1, 1, 0,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);

            CD3DX12_HEAP_PROPERTIES heapDefault(D3D12_HEAP_TYPE_DEFAULT);
            ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateCommittedResource(
                &heapDefault, D3D12_HEAP_FLAG_NONE, &renderTargetDesc,
                D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&m_pAdaptersD3D[adapterIdx].m_renderTargets[n])));

            wchar_t name[25] = {};
            swprintf_s(name, L"Render target %u", n);
            m_pAdaptersD3D[adapterIdx].m_renderTargets[n]->SetName(name);

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = m_backBufferFormat;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        #ifdef __MINGW32__
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            std::ignore = m_pAdaptersD3D[adapterIdx].m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(&cpuHandle);
        #else
            auto cpuHandle = m_pAdaptersD3D[adapterIdx].m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        #endif

            const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(cpuHandle, static_cast<INT>(n), m_pAdaptersD3D[adapterIdx].m_rtvDescriptorSize);
            m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateRenderTargetView(m_pAdaptersD3D[adapterIdx].m_renderTargets[n].Get(), &rtvDesc, rtvDescriptor);
        }
    }

    // Reset the index to the current back buffer.
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
        {
            // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
            // on this surface.
            CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

            D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                m_depthBufferFormat,
                backBufferWidth,
                backBufferHeight,
                1, // This depth stencil view has only one texture.
                1  // Use a single mipmap level.
            );
            depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            const CD3DX12_CLEAR_VALUE depthOptimizedClearValue(m_depthBufferFormat, 1.0f, 0u);

            ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateCommittedResource(
                &depthHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &depthStencilDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depthOptimizedClearValue,
                IID_PPV_ARGS(m_pAdaptersD3D[adapterIdx].m_depthStencil.ReleaseAndGetAddressOf())
            ));
            m_pAdaptersD3D[adapterIdx].m_depthStencil->SetName(L"Depth stencil");

        #ifdef __MINGW32__
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            std::ignore = m_pAdaptersD3D[adapterIdx].m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(&cpuHandle);
        #else
            auto cpuHandle = m_pAdaptersD3D[adapterIdx].m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        #endif

            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = m_depthBufferFormat;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            m_pAdaptersD3D[adapterIdx].m_d3dDevice->CreateDepthStencilView(m_pAdaptersD3D[adapterIdx].m_depthStencil.Get(), &dsvDesc, cpuHandle);
        }
    }

    // Set the 3D rendering viewport and scissor rectangle to target the entire window.
    m_screenViewport.TopLeftX = m_screenViewport.TopLeftY = 0.f;
    m_screenViewport.Width = static_cast<float>(backBufferWidth);
    m_screenViewport.Height = static_cast<float>(backBufferHeight);
    m_screenViewport.MinDepth = D3D12_MIN_DEPTH;
    m_screenViewport.MaxDepth = D3D12_MAX_DEPTH;

    m_scissorRect.left = m_scissorRect.top = 0;
    m_scissorRect.right = static_cast<LONG>(backBufferWidth);
    m_scissorRect.bottom = static_cast<LONG>(backBufferHeight);
}

// This method is called when the Win32 window is created (or re-created).
void DeviceResources::SetWindow(HWND window, int width, int height) noexcept
{
    m_window = window;

    m_outputSize.left = m_outputSize.top = 0;
    m_outputSize.right = width;
    m_outputSize.bottom = height;
}

// This method is called when the Win32 window changes size.
bool DeviceResources::WindowSizeChanged(int width, int height)
{
    if (!m_window)
        return false;

    RECT newRc;
    newRc.left = newRc.top = 0;
    newRc.right = width;
    newRc.bottom = height;
    if (newRc.right == m_outputSize.right && newRc.bottom == m_outputSize.bottom)
    {
        // Handle color space settings for HDR
        UpdateColorSpace();

        return false;
    }

    m_outputSize = newRc;
    CreateWindowSizeDependentResources();
    return true;
}

// Recreate all device resources and set them back to the current state.
void DeviceResources::HandleDeviceLost()
{
    if (m_pAdaptersD3D[DT_Primary].m_deviceNotify)
    {
        m_pAdaptersD3D[DT_Primary].m_deviceNotify->OnDeviceLost();
    }

    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        for (UINT n = 0; n < m_backBufferCount; n++)
        {
            m_pAdaptersD3D[adapterIdx].m_commandAllocators[n].Reset();
            m_pAdaptersD3D[adapterIdx].m_renderTargets[n].Reset();
        }

        m_pAdaptersD3D[adapterIdx].m_depthStencil.Reset();
        m_pAdaptersD3D[adapterIdx].m_commandQueue.Reset();
        m_pAdaptersD3D[adapterIdx].m_commandList.Reset();
        m_pAdaptersD3D[adapterIdx].m_fence.Reset();
        m_pAdaptersD3D[adapterIdx].m_rtvDescriptorHeap.Reset();
        m_pAdaptersD3D[adapterIdx].m_dsvDescriptorHeap.Reset();
        m_pAdaptersD3D[adapterIdx].m_d3dDevice.Reset();
    }

    m_swapChain.Reset();
    m_dxgiFactory.Reset();

#if defined(_DEBUG) && !defined(__MINGW32__)
    {
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
#endif

    CreateDeviceResources();
    CreateWindowSizeDependentResources();

    if (m_pAdaptersD3D[DT_Primary].m_deviceNotify)
    {
        m_pAdaptersD3D[DT_Primary].m_deviceNotify->OnDeviceRestored();
    }
}

// Prepare the command list and render target for rendering.
void DeviceResources::Prepare(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        // Reset command list and allocator.
        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_commandAllocators[m_backBufferIndex]->Reset());
        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_commandList->Reset(m_pAdaptersD3D[adapterIdx].m_commandAllocators[m_backBufferIndex].Get(), nullptr));

        if (beforeState != afterState)
        {
            // Transition the render target into the correct state to allow for drawing into it.
            const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_pAdaptersD3D[adapterIdx].m_renderTargets[m_backBufferIndex].Get(),
                beforeState, afterState);
            m_pAdaptersD3D[adapterIdx].m_commandList->ResourceBarrier(1, &barrier);
        }
    }
}

// Present the contents of the swap chain to the screen.
void DeviceResources::Present(D3D12_RESOURCE_STATES beforeState)
{
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
        {
            // Transition the render target to the state that allows it to be presented to the display.
            const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_pAdaptersD3D[adapterIdx].m_renderTargets[m_backBufferIndex].Get(),
                beforeState, D3D12_RESOURCE_STATE_PRESENT);
            m_pAdaptersD3D[adapterIdx].m_commandList->ResourceBarrier(1, &barrier);
        }

        // Send the command list off to the GPU for processing.
        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_commandList->Close());
        m_pAdaptersD3D[adapterIdx].m_commandQueue->ExecuteCommandLists(1, CommandListCast(m_pAdaptersD3D[adapterIdx].m_commandList.GetAddressOf()));
    }

    HRESULT hr;
    if (m_options & c_AllowTearing)
    {
        // Recommended to always use tearing if supported when using a sync interval of 0.
        // Note this will fail if in true 'fullscreen' mode.
        hr = m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    else
    {
        // The first argument instructs DXGI to block until VSync, putting the application
        // to sleep until the next VSync. This ensures we don't waste any cycles rendering
        // frames that will never be displayed to the screen.
        hr = m_swapChain->Present(1, 0);
    }

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
        char buff[64] = {};
        sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n",
            static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_pAdaptersD3D[DT_Primary].m_d3dDevice->GetDeviceRemovedReason() : hr));
        OutputDebugStringA(buff);
#endif
        HandleDeviceLost();
    }
    else
    {
        ThrowIfFailed(hr);

        MoveToNextFrame();

        if (!m_dxgiFactory->IsCurrent())
        {
            UpdateColorSpace();
        }
    }
}

// Wait for pending GPU work to complete.
void DeviceResources::WaitForGpu() noexcept
{
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        if (m_pAdaptersD3D[adapterIdx].m_commandQueue && m_pAdaptersD3D[adapterIdx].m_fence && m_pAdaptersD3D[adapterIdx].m_fenceEvent.IsValid())
        {
            // Schedule a Signal command in the GPU queue.
            const UINT64 fenceValue = m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex];
            if (SUCCEEDED(m_pAdaptersD3D[adapterIdx].m_commandQueue->Signal(m_pAdaptersD3D[adapterIdx].m_fence.Get(), fenceValue)))
            {
                // Wait until the Signal has been processed.
                if (SUCCEEDED(m_pAdaptersD3D[adapterIdx].m_fence->SetEventOnCompletion(fenceValue, m_pAdaptersD3D[adapterIdx].m_fenceEvent.Get())))
                {
                    WaitForSingleObjectEx(m_pAdaptersD3D[adapterIdx].m_fenceEvent.Get(), INFINITE, FALSE);

                    // Increment the fence value for the current frame.
                    m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex]++;
                }
            }
        }
    }
}

// Prepare to render the next frame.
void DeviceResources::MoveToNextFrame()
{
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceCount; ++adapterIdx)
    {
        // Schedule a Signal command in the queue.
        const UINT64 currentFenceValue = m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex];
        ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_commandQueue->Signal(m_pAdaptersD3D[adapterIdx].m_fence.Get(), currentFenceValue));

        // Update the back buffer index.
        m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

        // If the next frame is not ready to be rendered yet, wait until it is ready.
        if (m_pAdaptersD3D[adapterIdx].m_fence->GetCompletedValue() < m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex])
        {
            ThrowIfFailed(m_pAdaptersD3D[adapterIdx].m_fence->SetEventOnCompletion(m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex], m_pAdaptersD3D[adapterIdx].m_fenceEvent.Get()));
            WaitForSingleObjectEx(m_pAdaptersD3D[adapterIdx].m_fenceEvent.Get(), INFINITE, FALSE);
        }

        // Set the fence value for the next frame.
        m_pAdaptersD3D[adapterIdx].m_fenceValues[m_backBufferIndex] = currentFenceValue + 1;
    }
}

// This method acquires the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, try WARP. Otherwise throw an exception.
void DeviceResources::GetAdapter(IDXGIAdapter1* ppAdapters[], unsigned int numAdapters)
{
    for (unsigned int adapterIdx = 0; adapterIdx != numAdapters; ++adapterIdx)
    {
        ppAdapters[adapterIdx] = nullptr;
    }

    unsigned int numFoundAdapters = 0;

    if (!s_debugForceWarp)
    {
        ComPtr<IDXGIFactory6> factory6;
        HRESULT hr = m_dxgiFactory.As(&factory6);
        if (SUCCEEDED(hr))
        {
            for (UINT adapterIndex = 0;
                DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
                    adapterIndex,
                    s_debugPreferMinPower ? DXGI_GPU_PREFERENCE_MINIMUM_POWER : DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(&(ppAdapters[numFoundAdapters])));
                adapterIndex++)
            {
                DXGI_ADAPTER_DESC1 desc;
                ppAdapters[numFoundAdapters]->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

                // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(ppAdapters[numFoundAdapters], m_d3dMinFeatureLevel, __uuidof(ID3D12Device), nullptr)))
                {
                    if (s_debugAdapterOrdinal == -1 || (s_debugAdapterOrdinal == int(adapterIndex)))
                    {
#ifdef _DEBUG
                        wchar_t buff[256] = {};
                        swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                        OutputDebugStringW(buff);
#endif
                        ++numFoundAdapters;
                        if (numFoundAdapters == numAdapters)
                        {
                            break; //We found enough adapters.
                        }
                    }
                }
            }
        }
        else
        {
            for (UINT adapterIndex = 0;
                DXGI_ERROR_NOT_FOUND != m_dxgiFactory->EnumAdapters1(
                    adapterIndex,
                    &(ppAdapters[numFoundAdapters]));
                ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                ThrowIfFailed(ppAdapters[numFoundAdapters]->GetDesc1(&desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

                // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(ppAdapters[numFoundAdapters], m_d3dMinFeatureLevel, __uuidof(ID3D12Device), nullptr)))
                {
                    if (s_debugAdapterOrdinal == -1 || (s_debugAdapterOrdinal == int(adapterIndex)))
                    {
                        wchar_t buff[256] = {};
                        swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                        OutputDebugStringW(buff);

                        ++numFoundAdapters;
                        if (numFoundAdapters == numAdapters)
                        {
                            break; //We found enough adapters.
                        }
                    }
                }
            }
        }
    }

    if (numFoundAdapters != numAdapters)
    {
        for (UINT starting = numFoundAdapters; starting != numAdapters; ++starting)
        {
            // Try WARP12 instead
            if (FAILED(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&(ppAdapters[starting])))))
            {
                throw std::runtime_error("WARP12 not available. Enable the 'Graphics Tools' optional feature");
            }
            ++numFoundAdapters;
            OutputDebugStringA("Adding Direct3D Adapter - WARP12\n");
        }
    }

    assert(numFoundAdapters == numAdapters);

}

// Sets the color space for the swap chain in order to handle HDR output.
void DeviceResources::UpdateColorSpace()
{
    if (!m_dxgiFactory)
        return;

    if (!m_dxgiFactory->IsCurrent())
    {
        // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
        ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));
    }

    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    bool isDisplayHDR10 = false;

    if (m_swapChain)
    {
        // To detect HDR support, we will need to check the color space in the primary
        // DXGI output associated with the app at this point in time
        // (using window/display intersection).

        // Get the retangle bounds of the app window.
        RECT windowBounds;
        if (!GetWindowRect(m_window, &windowBounds))
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "GetWindowRect");

        const long ax1 = windowBounds.left;
        const long ay1 = windowBounds.top;
        const long ax2 = windowBounds.right;
        const long ay2 = windowBounds.bottom;

        ComPtr<IDXGIOutput> bestOutput;
        long bestIntersectArea = -1;

        ComPtr<IDXGIAdapter> adapter;
        for (UINT adapterIndex = 0;
            SUCCEEDED(m_dxgiFactory->EnumAdapters(adapterIndex, adapter.ReleaseAndGetAddressOf()));
            ++adapterIndex)
        {
            ComPtr<IDXGIOutput> output;
            for (UINT outputIndex = 0;
                SUCCEEDED(adapter->EnumOutputs(outputIndex, output.ReleaseAndGetAddressOf()));
                ++outputIndex)
            {
                // Get the rectangle bounds of current output.
                DXGI_OUTPUT_DESC desc;
                ThrowIfFailed(output->GetDesc(&desc));
                const auto& r = desc.DesktopCoordinates;

                // Compute the intersection
                const long intersectArea = ComputeIntersectionArea(ax1, ay1, ax2, ay2, r.left, r.top, r.right, r.bottom);
                if (intersectArea > bestIntersectArea)
                {
                    bestOutput.Swap(output);
                    bestIntersectArea = intersectArea;
                }
            }
        }

        if (bestOutput)
        {
            ComPtr<IDXGIOutput6> output6;
            if (SUCCEEDED(bestOutput.As(&output6)))
            {
                DXGI_OUTPUT_DESC1 desc;
                ThrowIfFailed(output6->GetDesc1(&desc));

                if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                {
                    // Display output is HDR10.
                    isDisplayHDR10 = true;
                }
            }
        }
    }

    if ((m_options & c_EnableHDR) && isDisplayHDR10)
    {
        switch (m_backBufferFormat)
        {
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            // The application creates the HDR10 signal.
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            break;

        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            // The system creates the HDR10 signal; application uses linear values.
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            break;

        default:
            break;
        }
    }

    m_colorSpace = colorSpace;

    UINT colorSpaceSupport = 0;
    if (m_swapChain
        && SUCCEEDED(m_swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport))
        && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
    {
        ThrowIfFailed(m_swapChain->SetColorSpace1(colorSpace));
    }
}
