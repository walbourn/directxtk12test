//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK - HLSL shader coverage
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#include "DirectXTKTest.h"
#include "StepTimer.h"

#include "RenderTexture.h"

constexpr uint32_t c_testTimeout = 10000;

// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game
#ifdef LOSTDEVICE
    final : public DX::IDeviceNotify
#endif
{
public:

    Game() noexcept(false);
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
#ifdef COREWINDOW
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);
#else
    void Initialize(HWND window, int width, int height, DXGI_MODE_ROTATION rotation);
#endif

    // Basic game loop
    void Tick();

#ifdef LOSTDEVICE
    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;
#endif

    // Messages
    void OnActivated();
    void OnDeactivated() {}
    void OnSuspending();
    void OnResuming();

#ifdef PC
    void OnWindowMoved();
#endif

#if defined(PC) || defined(UWP)
    void OnDisplayChange();
#endif

#ifndef XBOX
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
#endif

#ifdef UWP
    void ValidateDevice();
#endif

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
    const wchar_t* GetAppName() const { return L"ShaderTest (DirectX 12)"; }
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void CycleRenderMode();

    void CreateCube();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    // DirectXTK Test Objects
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    std::unique_ptr<DirectX::CommonStates>      m_states;
    std::unique_ptr<DirectX::DescriptorHeap>    m_resourceDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap>    m_renderDescriptors;

    std::vector<std::unique_ptr<DirectX::BasicEffect>> m_basic;
    std::vector<std::unique_ptr<DirectX::BasicEffect>> m_basicBn;
    std::vector<std::unique_ptr<DirectX::SkinnedEffect>> m_skinning;
    std::vector<std::unique_ptr<DirectX::SkinnedEffect>> m_skinningBn;
    std::vector<std::unique_ptr<DirectX::EnvironmentMapEffect>> m_envmap;
    std::vector<std::unique_ptr<DirectX::EnvironmentMapEffect>> m_envmapBn;
    std::vector<std::unique_ptr<DirectX::DualTextureEffect>> m_dual;
    std::vector<std::unique_ptr<DirectX::AlphaTestEffect>> m_alphTest;
    std::vector<std::unique_ptr<DirectX::NormalMapEffect>> m_normalMap;
    std::vector<std::unique_ptr<DirectX::NormalMapEffect>> m_normalMapBn;
    std::vector<std::unique_ptr<DirectX::SkinnedNormalMapEffect>> m_skinningNormalMap;
    std::vector<std::unique_ptr<DirectX::SkinnedNormalMapEffect>> m_skinningNormalMapBn;
    std::vector<std::unique_ptr<DirectX::PBREffect>> m_pbr;
    std::vector<std::unique_ptr<DirectX::PBREffect>> m_pbrBn;
    std::vector<std::unique_ptr<DirectX::SkinnedPBREffect>> m_skinningPbr;
    std::vector<std::unique_ptr<DirectX::SkinnedPBREffect>> m_skinningPbrBn;
    std::vector<std::unique_ptr<DirectX::DebugEffect>> m_debug;
    std::vector<std::unique_ptr<DirectX::DebugEffect>> m_debugBn;

    std::vector<std::unique_ptr<DirectX::NormalMapEffect>> m_normalMapInstanced;
    std::vector<std::unique_ptr<DirectX::NormalMapEffect>> m_normalMapInstancedBn;
    std::vector<std::unique_ptr<DirectX::PBREffect>> m_pbrInstanced;
    std::vector<std::unique_ptr<DirectX::PBREffect>> m_pbrInstancedBn;
    std::vector<std::unique_ptr<DirectX::DebugEffect>> m_debugInstanced;
    std::vector<std::unique_ptr<DirectX::DebugEffect>> m_debugInstancedBn;

    std::unique_ptr<DX::RenderTexture>              m_velocityBuffer;

    UINT					                        m_indexCount;
    UINT                                            m_instanceCount;
    DirectX::GraphicsResource		                m_indexBuffer;
    DirectX::GraphicsResource		                m_vertexBuffer;
    DirectX::GraphicsResource		                m_vertexBufferBn;

    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;
    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferViewBn;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cat;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cubemap;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_envball;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_envdual;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_overlay;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_defaultTex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickDiffuse;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickNormal;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickSpecular;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_pbrAlbedo;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_pbrNormal;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_pbrRMA;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_pbrEmissive;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_radianceIBL;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_irradianceIBL;

    std::unique_ptr<DirectX::XMFLOAT3X4[]>          m_instanceTransforms;

    enum RenderMode
    {
        Render_Normal,
        Render_Compressed,
        Render_Instanced,
        Render_CompressedInstanced,
        Render_Max
    };

    unsigned int                                    m_renderMode;
    float                                           m_delay;
    uint64_t                                        m_frame;

    enum Descriptors
    {
        Cat,
        Cubemap,
        SphereMap,
        DualParabolaMap,
        Overlay,
        DefaultTex,
        BrickDiffuse,
        BrickNormal,
        BrickSpecular,
        PBRAlbedo,
        PBRNormal,
        PBR_RMA,
        PBREmissive,
        RadianceIBL,
        IrradianceIBL,
        VelocityBuffer,
        Count
    };

    enum RTDescriptors
    {
        RTVelocityBuffer,
        RTCount
    };
};
