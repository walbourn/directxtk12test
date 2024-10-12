//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for basic Direct3D 12 support
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#pragma once

#include "DirectXTKTest.h"
#include "StepTimer.h"

constexpr uint32_t c_testTimeout = 5000;

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
    const wchar_t* GetAppName() const { return L"D3D12Test (DirectX 12)"; }
    bool RequestHDRMode() const { return false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void UnitTests();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    // DirectXTK Test Objects
    std::unique_ptr<DirectX::GraphicsMemory>            m_graphicsMemory;
    std::unique_ptr<DirectX::BasicEffect>               m_effectPoint;
    std::unique_ptr<DirectX::BasicEffect>               m_effectLine;
    std::unique_ptr<DirectX::BasicEffect>               m_effectTri;

    using Vertex = DirectX::VertexPositionColor;
    std::unique_ptr<DirectX::PrimitiveBatch<Vertex>>    m_batch;

    std::unique_ptr<DirectX::CommonStates>              m_states;
    std::unique_ptr<DirectX::DescriptorHeap>            m_resourceDescriptors;

    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test1;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test2;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test3;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test4;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test5;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test6;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test7;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test8;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test9;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_test10;

    uint64_t m_frame;
};
