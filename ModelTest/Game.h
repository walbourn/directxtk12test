//
// Game.h
//

#pragma once

#include "DeviceResourcesPC.h"
#include "StepTimer.h"

#include "PlatformHelpers.h"

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
    const wchar_t* GetAppName() const { return L"ModelTest (DirectX 12)"; }

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
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::DescriptorHeap>        m_resourceDescriptors;
  
    std::unique_ptr<DirectX::Model>                 m_cup;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupNormal;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupCustom;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupWireframe;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupFog;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_cupVertexLighting;

    std::unique_ptr<DirectX::Model>                 m_vbo;
    std::unique_ptr<DirectX::BasicEffect>           m_vboNormal;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_vboEnvMap;

    std::unique_ptr<DirectX::Model>                 m_tiny;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_tinyNormal;

    std::unique_ptr<DirectX::Model>                 m_solider;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_soldierNormal;

    std::unique_ptr<DirectX::Model>                 m_dwarf;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_dwarfNormal;

    std::unique_ptr<DirectX::Model>                 m_lmap;
    std::vector<std::shared_ptr<DirectX::IEffect>>  m_lmapNormal;

    std::unique_ptr<DirectX::EffectTextureFactory>  m_modelResources;
    std::unique_ptr<DirectX::EffectFactory>         m_fxFactory;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_defaultTex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cubemap;

    DirectX::SimpleMath::Matrix                     m_view;
    DirectX::SimpleMath::Matrix                     m_projection;

    std::unique_ptr<DirectX::XMMATRIX[], DirectX::aligned_deleter> m_bones;

    enum Descriptors
    {
        DefaultTex = 0,
        Cubemap = 1,
        ModelStart = 10,
        Count = 100,
    };
};