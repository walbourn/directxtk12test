//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK DDSTextureLoader & WICTextureLoader
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
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
    std::unique_ptr<DirectX::DescriptorHeap>        m_renderDescriptors;
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
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test27;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test28;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test29;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test30;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test31;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test32;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test33;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test34;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test35;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test36;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test37;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test38;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_test39;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_testA;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_testB;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_testC;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_testD;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_testE;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_testF;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_copyQueue;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_copyTest;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_computeQueue;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_computeTest;

    enum Descriptors
    {
        Earth,
        Earth_Imm,
        DirectXLogo,
        DirectXLogo_BC1,
        Windows95,
        Windows95_sRGB,
        Win95_UAV,
        Count
    };

    enum RTDescriptors
    {
        Win95_RTV,
        RTCount
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_screenshot;

    uint64_t m_frame;

    bool m_firstFrame;
};
