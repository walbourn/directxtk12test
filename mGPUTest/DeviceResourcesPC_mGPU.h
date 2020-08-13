//
// DeviceResources.h - A wrapper for the Direct3D 12 device and swapchain (mGPU variant)
//

#pragma once

namespace DX
{
    enum DeviceType { DT_Primary };

    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    interface IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;

    protected:
        ~IDeviceNotify() = default;
    };

    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static const unsigned int c_AllowTearing    = 0x1;
        static const unsigned int c_EnableHDR       = 0x2;

        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        UINT backBufferCount = 2,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0,
                        unsigned int flags = 0,
                        unsigned int deviceCount = 1) noexcept(false);
        ~DeviceResources();

        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND window, int width, int height) noexcept;
        bool WindowSizeChanged(int width, int height);
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) noexcept { m_pAdaptersD3D[0].m_deviceNotify = deviceNotify; }
        void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT,
                     D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_RENDER_TARGET);
        void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);
        void WaitForGpu() noexcept;

        // Device Accessors.
        RECT GetOutputSize() const noexcept { return m_outputSize; }

        // Direct3D Accessors to features common to all devices.
        auto                        GetSwapChain() const  noexcept         { return m_swapChain.Get(); }
        D3D_FEATURE_LEVEL           GetDeviceFeatureLevel() const noexcept { return m_d3dFeatureLevel; }
        DXGI_FORMAT                 GetBackBufferFormat() const noexcept   { return m_backBufferFormat; }
        DXGI_FORMAT                 GetDepthBufferFormat() const noexcept  { return m_depthBufferFormat; }
        D3D12_VIEWPORT              GetScreenViewport() const noexcept     { return m_screenViewport; }
        D3D12_RECT                  GetScissorRect() const noexcept        { return m_scissorRect; }
        UINT                        GetCurrentFrameIndex() const noexcept  { return m_backBufferIndex; }
        UINT                        GetBackBufferCount() const noexcept    { return m_backBufferCount; }
        UINT                        GetDeviceCount() const noexcept        { return m_deviceCount; }
        DXGI_COLOR_SPACE_TYPE       GetColorSpace() const noexcept         { return m_colorSpace; }
        unsigned int                GetDeviceOptions() const noexcept      { return m_options; }

        // Direct3D Accessors to features that are per device.
        ID3D12Device*                           GetD3DDevice(unsigned int idx = 0) const noexcept        { return m_pAdaptersD3D[idx].m_d3dDevice.Get(); }
        ID3D12Resource*                         GetRenderTarget(unsigned int idx = 0) const noexcept     { return m_pAdaptersD3D[idx].m_renderTargets[m_backBufferIndex].Get(); }
        ID3D12Resource*                         GetDepthStencil(unsigned int idx = 0) const noexcept     { return m_pAdaptersD3D[idx].m_depthStencil.Get(); }
        ID3D12CommandQueue*                     GetCommandQueue(unsigned int idx = 0) const noexcept     { return m_pAdaptersD3D[idx].m_commandQueue.Get(); }
        ID3D12CommandAllocator*                 GetCommandAllocator(unsigned int idx = 0) const noexcept { return m_pAdaptersD3D[idx].m_commandAllocators[m_backBufferIndex].Get(); }
        ID3D12GraphicsCommandList*              GetCommandList(unsigned int idx = 0) const noexcept      { return m_pAdaptersD3D[idx].m_commandList.Get(); }

        CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView(int idx = 0) const noexcept
        {
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(
                m_pAdaptersD3D[idx].m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                static_cast<INT>(m_backBufferIndex), m_pAdaptersD3D[idx].m_rtvDescriptorSize);
        }
        
        CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView(int idx = 0) const noexcept
        {
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pAdaptersD3D[idx].m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        }

        static void DebugForceWarp(bool enable) noexcept
        {
            s_debugForceWarp = enable;
        }

        static void DebugPreferMinimumPower(bool enable) noexcept
        {
            s_debugPreferMinPower = enable;
        }

        static void DebugSetAdapter(int adapter) noexcept
        {
            s_debugAdapterOrdinal = adapter;
        }

    private:
        void MoveToNextFrame();
        void GetAdapter(IDXGIAdapter1* ppAdapters[], unsigned int numAdapters = 1);
        void UpdateColorSpace();

        static const size_t MAX_BACK_BUFFER_COUNT = 3;

        UINT                                                m_backBufferIndex;

        UINT                                                m_deviceCount;
        D3D12_VIEWPORT                                      m_screenViewport;
        D3D12_RECT                                          m_scissorRect;

        // Swap chain objects.
        Microsoft::WRL::ComPtr<IDXGIFactory4>               m_dxgiFactory;
        Microsoft::WRL::ComPtr<IDXGISwapChain3>             m_swapChain;


        // Direct3D properties.
        DXGI_FORMAT                                         m_backBufferFormat;
        DXGI_FORMAT                                         m_depthBufferFormat;
        UINT                                                m_backBufferCount;
        D3D_FEATURE_LEVEL                                   m_d3dMinFeatureLevel;

        // Cached device properties.
        HWND                                                m_window;
        D3D_FEATURE_LEVEL                                   m_d3dFeatureLevel;
        DWORD                                               m_dxgiFactoryFlags;
        RECT                                                m_outputSize;

        // HDR Support
        DXGI_COLOR_SPACE_TYPE                               m_colorSpace;

        // DeviceResources options (see flags above)
        unsigned int                                        m_options;

        // Hold features that are per device in a multi GPU setup.
        struct PerAdapter
        {
            // Direct3D objects.
            Microsoft::WRL::ComPtr<ID3D12Device>                    m_d3dDevice;
            // The IDeviceNotify can be held directly as it owns the DeviceResources.
            IDeviceNotify*                                          m_deviceNotify;

            // Direct3D rendering objects.
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>            m_rtvDescriptorHeap;
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>            m_dsvDescriptorHeap;
            UINT                                                    m_rtvDescriptorSize;

            // Presentation fence objects.
            Microsoft::WRL::ComPtr<ID3D12Fence>                     m_fence;
            Microsoft::WRL::Wrappers::Event                         m_fenceEvent;
            UINT64                                                  m_fenceValues[MAX_BACK_BUFFER_COUNT];

            // Swap chain objects.
            Microsoft::WRL::ComPtr<ID3D12Resource>                  m_renderTargets[MAX_BACK_BUFFER_COUNT];
            Microsoft::WRL::ComPtr<ID3D12Resource>                  m_depthStencil;

            // Command
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator>          m_commandAllocators[MAX_BACK_BUFFER_COUNT];
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>       m_commandList;
            Microsoft::WRL::ComPtr<ID3D12CommandQueue>              m_commandQueue;

            PerAdapter() :
                m_deviceNotify(nullptr),
                m_rtvDescriptorSize(0),
                m_fenceValues{}
            {}
        };
        PerAdapter*											    m_pAdaptersD3D;

        // Debugging options
        static bool s_debugForceWarp;
        static bool s_debugPreferMinPower;
        static int s_debugAdapterOrdinal;
    };
}
