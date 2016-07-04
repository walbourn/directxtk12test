//
// Game.h
//

#pragma once

#include "DeviceResourcesPC.h"
#include "StepTimer.h"


// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game : public DX::IDeviceNotify
{
public:

    Game();
    ~Game();

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();
    void Render();

    // Rendering helpers
    void Clear();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
    const wchar_t* GetAppName() const { return L"PrimitivesTest (DirectX 12)"; }

private:

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    // DirectXTK Test Objects
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;
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

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cat;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_dxLogo;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_refTexture;

    enum Descriptors
    {
        Cat,
        DirectXLogo,
        RefTexture,
        Count
    };
};