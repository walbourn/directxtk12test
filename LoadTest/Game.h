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
    const wchar_t* GetAppName() const { return L"D3D12Test (DirectX 12)"; }

private:

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void UnitTests(DirectX::ResourceUploadBatch& resourceUpload, bool success);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
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

    enum Descriptors
    {
        Earth,
        Earth_sRGB,
        DirectXLogo,
        DirectXLogo_BC1,
        Windows95,
        Windows95_sRGB,
        Count
    };

};