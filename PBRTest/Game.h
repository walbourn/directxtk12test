//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK PBREffect
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#if defined(_XBOX_ONE) && defined(_TITLE)
#include "DeviceResourcesXDK.h"
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#include "DeviceResourcesUWP.h"
#else
#include "DeviceResourcesPC.h"
#endif
#include "StepTimer.h"

#include "RenderTexture.h"

// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game
#if !defined(_XBOX_ONE) || !defined(_TITLE)
    : public DX::IDeviceNotify
#endif
{
public:

    Game();
    ~Game();

    // Initialization and management
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
    void Initialize(HWND window, int width, int height, DXGI_MODE_ROTATION rotation);
#else
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);
#endif

    // Basic game loop
    void Tick();

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;
#endif

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
    void OnWindowMoved();
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    void ValidateDevice();
#endif

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
    const wchar_t* GetAppName() const { return L"PBRTest (DirectX 12)"; }
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

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
    std::unique_ptr<DirectX::DescriptorHeap>    m_resourceDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap>    m_renderDescriptors;
    std::unique_ptr<DirectX::CommonStates>      m_states;

    // HDR resources
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMap;

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMapLinear;
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMapHDR10;
#endif

    std::unique_ptr<DX::RenderTexture>              m_hdrScene;

    // Test geometry
    UINT                                            m_indexCount;
    DirectX::GraphicsResource                       m_indexBuffer;
    DirectX::GraphicsResource                       m_vertexBuffer;

    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferView;

    UINT                                            m_indexCountCube;
    DirectX::GraphicsResource                       m_indexBufferCube;
    DirectX::GraphicsResource                       m_vertexBufferCube;

    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferViewCube;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferViewCube;

    // Test materials
    std::unique_ptr<DirectX::NormalMapEffect>       m_normalMapEffect;
    std::unique_ptr<DirectX::PBREffect>             m_pbr;
    std::unique_ptr<DirectX::PBREffect>             m_pbrConstant;
    std::unique_ptr<DirectX::PBREffect>             m_pbrEmissive;
    std::unique_ptr<DirectX::PBREffect>             m_pbrCube;
    std::unique_ptr<DirectX::DebugEffect>           m_debug;
    std::unique_ptr<DirectX::DebugEffect>           m_debugN;
    std::unique_ptr<DirectX::DebugEffect>           m_debugT;
    std::unique_ptr<DirectX::DebugEffect>           m_debugB;

    static const size_t s_nMaterials = 3;
    static const size_t s_nIBL = 3;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_baseColor[s_nMaterials];
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_normalMap[s_nMaterials];
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_rma[s_nMaterials];
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_emissiveMap[s_nMaterials];

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_radianceIBL[s_nIBL];
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_irradianceIBL[s_nIBL];

    uint32_t m_ibl;
    bool m_spinning;
    bool m_showDebug;
    DirectX::DebugEffect::Mode m_debugMode;
    float m_pitch;
    float m_yaw;

    enum Descriptors
    {
        SceneTex,
        BaseColor1,
        BaseColor2,
        BaseColor3,
        NormalMap1,
        NormalMap2,
        NormalMap3,
        RMA1,
        RMA2,
        RMA3,
        RadianceIBL1,
        RadianceIBL2,
        RadianceIBL3,
        IrradianceIBL1,
        IrradianceIBL2,
        IrradianceIBL3,
        EmissiveTexture1,
        EmissiveTexture2,
        EmissiveTexture3,
        Count
    };

    enum RTDescriptors
    {
        HDRScene,
        RTCount
    };

    void CycleDebug();
};