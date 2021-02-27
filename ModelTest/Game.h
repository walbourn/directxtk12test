//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK Model
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#include "DirectXTKTest.h"
#include "StepTimer.h"

#include "PlatformHelpers.h"


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
    const wchar_t* GetAppName() const { return L"ModelTest (DirectX 12)"; }
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
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::DescriptorPile>        m_resourceDescriptors;

    std::unique_ptr<DirectX::Model>                 m_cup;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupNormal;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupCustom;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupWireframe;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupFog;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupVertexLighting;

    std::unique_ptr<DirectX::Model>                 m_cupMesh;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupMeshNormal;

    std::unique_ptr<DirectX::Model>                 m_vbo;
    std::unique_ptr<DirectX::BasicEffect>           m_vboNormal;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_vboEnvMap;

    std::unique_ptr<DirectX::Model>                 m_tiny;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_tinyNormal;

    std::unique_ptr<DirectX::Model>                 m_soldier;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_soldierNormal;

    std::unique_ptr<DirectX::Model>                 m_dwarf;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_dwarfNormal;

    std::unique_ptr<DirectX::Model>                 m_lmap;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_lmapNormal;

    std::unique_ptr<DirectX::Model>                 m_nmap;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_nmapNormal;

    std::unique_ptr<DirectX::IEffectTextureFactory> m_abstractModelResources;
    std::unique_ptr<DirectX::EffectTextureFactory>  m_modelResources;

    std::unique_ptr<DirectX::IEffectFactory>        m_abstractFXFactory;
    std::unique_ptr<DirectX::EffectFactory>         m_fxFactory;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_defaultTex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cubemap;

    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_projection;

    std::unique_ptr<DirectX::XMMATRIX[], DirectX::aligned_deleter> m_bones;

    bool m_spinning;
    bool m_firstFrame;
    float m_pitch;
    float m_yaw;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_copyQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_computeQueue;

    enum StaticDescriptors
    {
        DefaultTex = 0,
        Cubemap = 1,
        Reserve
    };
};
