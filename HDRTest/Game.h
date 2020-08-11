//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK PostProcess (HDR10/ToneMap)
//
// Copyright (c) Microsoft Corporation. All rights reserved.
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
    const wchar_t* GetAppName() const { return L"HDRTest (DirectX 12)"; }
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
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap>    m_resourceDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap>    m_renderDescriptors;

    // HDR resources
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMap;

#ifndef XBOX
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMapLinear;
    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMapHDR10;
#endif

    std::unique_ptr<DX::RenderTexture>              m_hdrScene;

    // Test resources.
    std::unique_ptr<DirectX::SpriteBatch>           m_batch;
    std::unique_ptr<DirectX::SpriteFont>            m_font;

    std::unique_ptr<DirectX::GeometricPrimitive>    m_shape;
    std::unique_ptr<DirectX::CommonStates>          m_states;

    std::unique_ptr<DirectX::BasicEffect>           m_flatEffect;
    std::unique_ptr<DirectX::BasicEffect>           m_basicEffect;
    std::unique_ptr<DirectX::BasicEffect>           m_brightEffect;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_hdrImage1;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_hdrImage2;

    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_projection;

    enum Descriptors
    {
        SceneTex,
        Font,
        HDRImage1,
        HDRImage2,
        Count
    };

    enum RTDescriptors
    {
        HDRScene,
        RTCount
    };
};
