//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK12 XboxDDSTextureLoader
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#ifdef _GAMING_XBOX
#include "DeviceResourcesGXDK.h"
#elif defined(_XBOX_ONE) && defined(_TITLE)
#include "DeviceResourcesXDK.h"
#else
#error This test is Xbox One only
#endif
#include "StepTimer.h"

constexpr uint32_t c_testTimeout = 5000;

// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game
{
public:

    Game() noexcept(false);
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_GAMES)
    void Initialize(HWND window, int width, int height, DXGI_MODE_ROTATION rotation);
#else
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);
#endif

    // Basic game loop
    void Tick();

    // Messages
    void OnActivated() {}
    void OnDeactivated() {}
    void OnSuspending();
    void OnResuming();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
    const wchar_t* GetAppName() const { return L"XboxLoadTest (DirectX 12)"; }
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void UnitTests(bool success);

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
    std::unique_ptr<DirectX::DescriptorHeap>        m_resourceDescriptors;
    std::unique_ptr<DirectX::BasicEffect>           m_effect;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_cube;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_earth;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_earth2;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_dxlogo;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_dxlogo2;

    enum Descriptors
    {
        Earth,
        Earth2,
        DirectXLogo,
        DirectXLogo2,
        Count
    };

    uint64_t m_frame;
};
