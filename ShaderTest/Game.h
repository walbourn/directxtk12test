//--------------------------------------------------------------------------------------
// File: Game.h
//
// Developer unit test for DirectXTK - HLSL shader coverage
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
    : public DX::IDeviceNotify
#endif
{
public:

    Game();
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

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    void ValidateDevice();
#endif

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
    const wchar_t* GetAppName() const { return L"ShaderTest (DirectX 12)"; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void CreateCube();

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
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    std::unique_ptr<DirectX::CommonStates>      m_states;
    std::unique_ptr<DirectX::DescriptorHeap>    m_resourceDescriptors;

    std::vector<std::unique_ptr<DirectX::BasicEffect>> m_basic;
    std::vector<std::unique_ptr<DirectX::BasicEffect>> m_basicBn;
    std::vector<std::unique_ptr<DirectX::SkinnedEffect>> m_skinning;
    std::vector<std::unique_ptr<DirectX::SkinnedEffect>> m_skinningBn;
    std::vector<std::unique_ptr<DirectX::EnvironmentMapEffect>> m_envmap;
    std::vector<std::unique_ptr<DirectX::EnvironmentMapEffect>> m_envmapBn;
    std::vector<std::unique_ptr<DirectX::DualTextureEffect>> m_dual;
    std::vector<std::unique_ptr<DirectX::AlphaTestEffect>> m_alphTest;
    std::vector<std::unique_ptr<DirectX::NormalMapEffect>> m_normalMap;
    std::vector<std::unique_ptr<DirectX::NormalMapEffect>> m_normalMapBn;

    UINT					                        m_indexCount;
    DirectX::GraphicsResource		                m_indexBuffer;
    DirectX::GraphicsResource		                m_vertexBuffer;
    DirectX::GraphicsResource		                m_vertexBufferBn;

    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;
    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferViewBn;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cat;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cubemap;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_overlay;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_defaultTex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickDiffuse;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickNormal;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickSpecular;

    bool                                            m_showCompressed;

    float                                           m_delay;

    enum Descriptors
    {
        Cat,
        Cubemap,
        Overlay,
        DefaultTex,
        BrickDiffuse,
        BrickNormal,
        BrickSpecular,
        Count
    };
};