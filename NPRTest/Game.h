//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectX Tool Kit - NPREffect
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#include "DirectXTKTest.h"
#include "StepTimer.h"

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
    void OnActivated() {}
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
    const wchar_t* GetAppName() const { return L"NPRTest (DirectX 12)"; }
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void CreateTeapot();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    // DirectXTK Test Objects
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::CommonStates>      m_states;

    UINT                                        m_indexCount;
    DirectX::GraphicsResource                   m_indexBuffer;
    DirectX::GraphicsResource                   m_vertexBuffer;

    D3D12_VERTEX_BUFFER_VIEW                    m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW                     m_indexBufferView;

    // Cel shading (Mode_Cel)
    std::unique_ptr<DirectX::NPREffect>         m_celEffect;
    std::unique_ptr<DirectX::NPREffect>         m_celEffectBands2;
    std::unique_ptr<DirectX::NPREffect>         m_celEffectBands8;
    std::unique_ptr<DirectX::NPREffect>         m_celEffectNoSpecular;
    std::unique_ptr<DirectX::NPREffect>         m_celEffectVc;

    // Gooch shading (Mode_Gooch)
    std::unique_ptr<DirectX::NPREffect>         m_goochEffect;
    std::unique_ptr<DirectX::NPREffect>         m_goochEffectNoSpecular;
    std::unique_ptr<DirectX::NPREffect>         m_goochEffectVc;
    std::unique_ptr<DirectX::NPREffect>         m_goochEffectCustom;

    uint64_t m_frame;
};
