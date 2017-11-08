//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK Model
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

#include "PlatformHelpers.h"


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

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
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

    std::unique_ptr<DirectX::EffectTextureFactory>  m_modelResources;
    std::unique_ptr<DirectX::EffectFactory>         m_fxFactory;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_defaultTex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cubemap;

    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_projection;

    std::unique_ptr<DirectX::XMMATRIX[], DirectX::aligned_deleter> m_bones;

    enum StaticDescriptors
    {
        DefaultTex = 0,
        Cubemap = 1,
        Reserve
    };
};