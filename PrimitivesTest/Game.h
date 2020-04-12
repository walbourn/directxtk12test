//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK Geometric Primitives
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
    const wchar_t* GetAppName() const { return L"PrimitivesTest (DirectX 12)"; }
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

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    // DirectXTK Test Objects
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::DescriptorHeap>        m_resourceDescriptors;
    std::unique_ptr<DirectX::BasicEffect>           m_effect;
    std::unique_ptr<DirectX::BasicEffect>           m_effectWireframe;
    std::unique_ptr<DirectX::BasicEffect>           m_effectTexture;
    std::unique_ptr<DirectX::BasicEffect>           m_effectAlpha;
    std::unique_ptr<DirectX::BasicEffect>           m_effectPMAlphaTexture;
    std::unique_ptr<DirectX::BasicEffect>           m_effectAlphaTexture;
    std::unique_ptr<DirectX::BasicEffect>           m_effectLights;
    std::unique_ptr<DirectX::BasicEffect>           m_effectFog;

    std::unique_ptr<DirectX::GeometricPrimitive>    m_cube;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_box;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_sphere;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_geosphere;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_cylinder;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_cone;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_torus;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_teapot;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_tetra;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_octa;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_dodec;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_iso;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_customBox;
    std::unique_ptr<DirectX::GeometricPrimitive>    m_customBox2;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cat;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_dxLogo;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_refTexture;

    bool m_spinning;
    bool m_firstFrame;
    float m_pitch;
    float m_yaw;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_copyQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_computeQueue;

    enum Descriptors
    {
        Cat,
        DirectXLogo,
        RefTexture,
        Count
    };
};
