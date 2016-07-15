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
    const wchar_t* GetAppName() const { return L"SpriteBatchTest (DirectX 12)"; }

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
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::CommonStates>      m_states;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch2;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch3;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch4;
    std::unique_ptr<DirectX::DescriptorHeap>    m_resourceDescriptors;

    Microsoft::WRL::ComPtr<ID3D12Resource>      m_cat;
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_letterA;
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_letterB;
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_letterC;

    enum Descriptors
    {
        Cat,
        A,
        B,
        C,
        D,
        Count
    };
};