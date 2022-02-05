//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK PostProcess
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

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
    const wchar_t* GetAppName() const { return L"PostProcessTest (DirectX 12)"; }
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>               m_gamePad;
    std::unique_ptr<DirectX::Keyboard>              m_keyboard;

    DirectX::GamePad::ButtonStateTracker            m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker         m_keyboardButtons;

    // DirectXTK Test Objects
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;

    std::unique_ptr<DirectX::DescriptorHeap>        m_resourceDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap>        m_rtvDescriptors;

    DirectX::SimpleMath::Matrix                     m_world;
    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_proj;

    int                                             m_scene;

    std::unique_ptr<DirectX::SpriteBatch>           m_spriteBatch;
    std::unique_ptr<DirectX::SpriteBatch>           m_spriteBatchUI;
    std::unique_ptr<DirectX::SpriteFont>            m_font;
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::BasicEffect>           m_effect;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_shape;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_texture;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_background;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_hdrTexture;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_sceneTex;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_blur1Tex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_blur2Tex;

    std::unique_ptr<DirectX::IPostProcess>          m_abstractPostProcess;
    std::unique_ptr<DirectX::BasicPostProcess>      m_basicPostProcess[DirectX::BasicPostProcess::Effect_Max];
    std::unique_ptr<DirectX::DualPostProcess>       m_dualPostProcess[DirectX::DualPostProcess::Effect_Max];

    static constexpr size_t ToneMapCount = static_cast<size_t>(DirectX::ToneMapPostProcess::Operator_Max) * static_cast<size_t>(DirectX::ToneMapPostProcess::TransferFunction_Max)
#ifdef XBOX
                                       * 2
#endif
                                       ;

    std::unique_ptr<DirectX::ToneMapPostProcess>    m_toneMapPostProcess[ToneMapCount];

    enum Descriptors
    {
        Font,
        Texture,
        Background,
        HDRTexture,
        SceneTex,
        Blur1Tex,
        Blur2Tex,
        Count
    };

    enum RTDescriptors
    {
        SceneRT,
        Blur1RT,
        Blur2RT,
        RTCount
    };

    float                                           m_delay;
};
