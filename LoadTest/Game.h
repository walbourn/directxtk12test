//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK DDSTextureLoader & WICTextureLoader
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


// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game
#if !defined(_XBOX_ONE) || !defined(_TITLE)
    final : public DX::IDeviceNotify
#endif
{
public:

    Game() noexcept(false);
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
    const wchar_t* GetAppName() const { return L"LoadTest (DirectX 12)"; }
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void UnitTests(DirectX::ResourceUploadBatch& resourceUpload, bool success);

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
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_win95;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_win95_2;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test1;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test2;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test3;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test4;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test5;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test6;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test7;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test8;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test9;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test10;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test11;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test12;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test13;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test14;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test15;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test16;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test17;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test18;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test19;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test20;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test21;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test22;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test23;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test24;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test25;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test26;

    enum Descriptors
    {
        Earth,
        Earth_Imm,
        DirectXLogo,
        DirectXLogo_BC1,
        Windows95,
        Windows95_sRGB,
        Count
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_screenshot;

    uint64_t m_frame;
};
