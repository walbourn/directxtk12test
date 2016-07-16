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
    const wchar_t* GetAppName() const { return L"EffectsTest (DirectX 12)"; }

private:

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void CreateTeapot();

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

    UINT					                        m_indexCount;
    DirectX::GraphicsResource		                m_indexBuffer;
    DirectX::GraphicsResource		                m_vertexBuffer;

    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferView;

    std::unique_ptr<DirectX::BasicEffect>           m_basicEffectUnlit;
    std::unique_ptr<DirectX::BasicEffect>           m_basicEffectUnlitFog;
    std::unique_ptr<DirectX::BasicEffect>           m_basicEffect;
    std::unique_ptr<DirectX::BasicEffect>           m_basicEffectFog;
    std::unique_ptr<DirectX::BasicEffect>           m_basicEffectNoSpecular;
    std::unique_ptr<DirectX::BasicEffect>           m_basicEffectPPL;

    std::unique_ptr<DirectX::SkinnedEffect>         m_skinnedEffect;
    std::unique_ptr<DirectX::SkinnedEffect>         m_skinnedEffectNoSpecular;

    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmap;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmapSpec;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmapFog;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmapNoFresnel;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmapPPL;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmapSpecPPL;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmapFogPPL;
    std::unique_ptr<DirectX::EnvironmentMapEffect>  m_envmapNoFresnelPPL;

    std::unique_ptr<DirectX::DualTextureEffect>     m_dualTexture;
    std::unique_ptr<DirectX::DualTextureEffect>     m_dualTextureFog;

    std::unique_ptr<DirectX::AlphaTestEffect>       m_alphaTest;
    std::unique_ptr<DirectX::AlphaTestEffect>       m_alphaTestFog;
    std::unique_ptr<DirectX::AlphaTestEffect>       m_alphaTestLess;
    std::unique_ptr<DirectX::AlphaTestEffect>       m_alphaTestEqual;
    std::unique_ptr<DirectX::AlphaTestEffect>       m_alphaTestNotEqual;

    std::unique_ptr<DirectX::NormalMapEffect>       m_normalMapEffect;
    std::unique_ptr<DirectX::NormalMapEffect>       m_normalMapEffectFog;
    std::unique_ptr<DirectX::NormalMapEffect>       m_normalMapEffectNoDiffuse;
    std::unique_ptr<DirectX::NormalMapEffect>       m_normalMapEffectNormalsOnly;
    std::unique_ptr<DirectX::NormalMapEffect>       m_normalMapEffectNoSpecular;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cat;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_opaqueCat;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cubemap;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_overlay;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_defaultTex;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickDiffuse;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickNormal;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_brickSpecular;

    enum Descriptors
    {
        Cat,
        OpaqueCat,
        Cubemap,
        Overlay,
        DefaultTex,
        BrickDiffuse,
        BrickNormal,
        BrickSpecular,
        Count
    };
};