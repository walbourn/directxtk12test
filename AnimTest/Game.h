//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK Model animation
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#include "Animation.h"
#include "DirectXTKTest.h"
#include "StepTimer.h"


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
    const wchar_t* GetAppName() const { return L"AnimTest (DirectX 12)"; }
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

    std::unique_ptr<DirectX::Model>                 m_soldier;
    DirectX::Model::EffectCollection                m_soldierNormal;
    DirectX::Model::EffectCollection                m_soldierDiffuse;

    std::unique_ptr<DirectX::Model>                 m_tank;
    DirectX::Model::EffectCollection                m_tankNormal;

    std::unique_ptr<DirectX::Model>                 m_teapot;
    DirectX::Model::EffectCollection                m_teapotNormal;

    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::DescriptorPile>        m_resourceDescriptors;
    std::unique_ptr<DirectX::EffectFactory>         m_fxFactory;
    std::unique_ptr<DirectX::EffectTextureFactory>  m_modelResources;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_defaultTex;

    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_projection;

    DirectX::ModelBone::TransformArray              m_bones;

    DX::AnimationSDKMESH                            m_soldierAnim;
    DX::AnimationCMO                                m_teapotAnim;

    enum StaticDescriptors
    {
        DefaultTex = 0,
        Reserve
    };
};
