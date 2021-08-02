//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK PBR Model Test
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
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();

#ifdef PC
    void OnWindowMoved();
#endif

#ifndef XBOX
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
#endif

#ifdef UWP
    void ValidateDevice();
#endif

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
    const wchar_t* GetAppName() const { return L"PBRModelTest (DirectX 12)"; }
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
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;
    std::unique_ptr<DirectX::DescriptorPile>        m_resourceDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap>        m_renderDescriptors;

    std::unique_ptr<DirectX::CommonStates>          m_states;

    std::unique_ptr<DirectX::Model>                 m_cube;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cubeNormal;

    std::unique_ptr<DirectX::Model>                 m_cubeInst;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cubeInstNormal;

    std::unique_ptr<DirectX::Model>                 m_sphere;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_sphereNormal;

    std::unique_ptr<DirectX::Model>                 m_sphere2;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_sphere2Normal;

    std::unique_ptr<DirectX::Model>                 m_robot;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_robotNormal;

    std::unique_ptr<DirectX::EffectTextureFactory>  m_modelResources;
    std::unique_ptr<DirectX::PBREffectFactory>      m_fxFactory;

    static const size_t s_nIBL = 3;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_radianceIBL[s_nIBL];
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_irradianceIBL[s_nIBL];

    // HDR resources
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMap;

#ifndef XBOX
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMapLinear;
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMapHDR10;
#endif

    std::unique_ptr<DX::RenderTexture>              m_hdrScene;

    enum Descriptors
    {
        SceneTex,
        RadianceIBL1,
        RadianceIBL2,
        RadianceIBL3,
        IrradianceIBL1,
        IrradianceIBL2,
        IrradianceIBL3,
        EmissiveTexture1,
        EmissiveTexture2,
        EmissiveTexture3,
        Reserve,
        Count = 128,
    };

    enum RTDescriptors
    {
        HDRScene,
        RTCount
    };

    DirectX::SimpleMath::Matrix             m_view;
    DirectX::SimpleMath::Matrix             m_projection;

    UINT                                    m_instanceCount;
    std::unique_ptr<DirectX::XMFLOAT3X4[]>  m_instanceTransforms;

    uint32_t m_ibl;
    bool m_spinning;
    float m_pitch;
    float m_yaw;
};
