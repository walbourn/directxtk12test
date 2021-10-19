//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for mGPU Direct3D 12 support
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#include "DeviceResourcesUWP_mGPU.h"
#else
#include "DeviceResourcesPC_mGPU.h"
#endif
#include "StepTimer.h"


// A basic game implementation that creates a D3D12 device and provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
    void Initialize(HWND window, int width, int height, DXGI_MODE_ROTATION rotation);
#else
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);
#endif

    // Basic game loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
    void OnWindowMoved();
#endif

    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    void ValidateDevice();
#endif

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
    const wchar_t* GetAppName() const { return L"D3D12Test (DirectX 12)"; }
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>									m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer															m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>										m_gamePad;
    std::unique_ptr<DirectX::Keyboard>										m_keyboard;

    // DirectXTK Test Objects
    static constexpr size_t MAX_DEVICES = 3;

    std::unique_ptr<DirectX::GraphicsMemory>                                m_graphicsMemory[MAX_DEVICES];
    std::unique_ptr<DirectX::BasicEffect>                                   m_effectPoint[MAX_DEVICES];
    std::unique_ptr<DirectX::BasicEffect>                                   m_effectLine[MAX_DEVICES];
    std::unique_ptr<DirectX::BasicEffect>                                   m_effectTri[MAX_DEVICES];
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>  m_batch[MAX_DEVICES];
};
