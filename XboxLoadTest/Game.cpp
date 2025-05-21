//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK DDSTextureLoader & WICTextureLoader
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

//#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float dist = 10.f;

#ifdef _GAMING_XBOX_SCARLETT
    const wchar_t* c_EARTH = L"earth_A2B10G10R10_scarlett.dds";
    const wchar_t* c_LOGO_AUTOGEN = L"dx5_logo_autogen_scarlett.dds";
    const wchar_t* c_LOGO = L"dx5_logo_scarlett.dds";
    const wchar_t* c_TREE_PMALPHA = L"tree02S_pmalpha_scarlett.dds";
#else
    const wchar_t* c_EARTH = L"earth_A2B10G10R10.dds";
    const wchar_t* c_LOGO_AUTOGEN = L"dx5_logo_autogen.dds";
    const wchar_t* c_LOGO = L"dx5_logo.dds";
    const wchar_t* c_TREE_PMALPHA = L"tree02S_pmalpha.dds";
#endif

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
}

// Constructor.
Game::Game() noexcept(false) :
    m_frame(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>();
#endif

    m_deviceResources->SetClearColor(c_clearColor);
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_GAMES)
    HWND window,
#else
    IUnknown* window,
#endif
    int width, int height, DXGI_MODE_ROTATION rotation)

{
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    UNREFERENCED_PARAMETER(rotation);

    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
#ifdef _GAMING_XBOX
    m_deviceResources->WaitForOrigin();
#endif

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    ++m_frame;
}

// Updates the world.
void Game::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    auto kb = m_keyboard->GetState();
    if (kb.Escape || (pad.IsConnected() && pad.IsViewPressed()))
    {
        ExitGame();
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    auto t = static_cast<float>(m_timer.GetTotalSeconds());

    // Cube 1
    XMMATRIX world = XMMatrixRotationY(t) * XMMatrixTranslation(1.5f, -1.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Earth), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 2
    world = XMMatrixRotationY(t) * XMMatrixTranslation(1.5f, 1.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Earth2), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 3
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(-1.5f, -1.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo2), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 4
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(-1.5f, 1.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    const auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    const auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, c_clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    const auto viewport = m_deviceResources->GetScreenViewport();
    const auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnSuspending()
{
    m_deviceResources->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_timer.ResetElapsedTime();
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    m_states = std::make_unique<CommonStates>(device);

#ifdef LH_COORDS
    m_cube = GeometricPrimitive::CreateCube(2.f, false);
#else
    m_cube = GeometricPrimitive::CreateCube(2.f, true);
#endif

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Texture, pd);
    }

    // View textures
    bool success = true;
    OutputDebugStringA("*********** UINT TESTS BEGIN ***************\n");
#ifdef _GAMING_XBOX_SCARLETT
    OutputDebugStringA("----------- Project Scarlett ---------------\n");
#else
    OutputDebugStringA("--------------- Xbox One -------------------\n");
#endif

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    using namespace Xbox;

    // Earth
    {
        DDS_ALPHA_MODE alphaMode = DDS_ALPHA_MODE_UNKNOWN;
        void* grfxMemory = nullptr;

        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, c_EARTH,
            m_earth.ReleaseAndGetAddressOf(), &grfxMemory, &alphaMode));

        if (alphaMode != DDS_ALPHA_MODE_OPAQUE)
        {
            OutputDebugStringA("FAILED: earth_A2B10G10R10.dds alpha mode value unexpected\n");
            success = false;
        }

        const auto desc = m_earth->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM
            || desc.Width != 512
            || desc.Height != 256
            || desc.MipLevels != 10)
        {
            OutputDebugStringA("FAILED: earth_A2B10G10R10.dds desc unexpected\n");
            success = false;
        }
    }

    DirectX::CreateShaderResourceView(device, m_earth.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth));

    {
        void* grfxMemory = nullptr;

        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, c_EARTH,
            m_earth2.ReleaseAndGetAddressOf(), &grfxMemory, nullptr, true));

        // forceSRGB has no effect for 10:10:10:2

        const auto desc = m_earth2->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM
            || desc.Width != 512
            || desc.Height != 256
            || desc.MipLevels != 10)
        {
            OutputDebugStringA("FAILED: earth_A2B10G10R10.dds (2) desc unexpected\n");
            success = false;
        }
    }

    DirectX::CreateShaderResourceView(device, m_earth2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth2));

    // DirectX Logo
    {
        void* grfxMemory = nullptr;

        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, c_LOGO_AUTOGEN,
            m_dxlogo.ReleaseAndGetAddressOf(), &grfxMemory));

        const auto desc = m_dxlogo->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: dx5_logo_autogen.dds desc unexpected\n");
            success = false;
        }
    }

    DirectX::CreateShaderResourceView(device, m_dxlogo.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo));

    {
        void* grfxMemory = nullptr;

        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, c_LOGO,
            m_dxlogo2.ReleaseAndGetAddressOf(), &grfxMemory, nullptr, true));

        const auto desc = m_dxlogo2->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_BC1_UNORM_SRGB
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: dx5_logo.dds (BC1 sRGB) desc unexpected\n");
            success = false;
        }
    }

    DirectX::CreateShaderResourceView(device, m_dxlogo2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo2));

    UnitTests(success);

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 eyePosition = { { { 0.0f, 3.0f, -6.0f, 0.0f } } };
    static const XMVECTORF32 At = { { { 0.0f, 1.0f, 0.0f, 0.0f } } };
    static const XMVECTORF32 Up = { { { 0.0f, 1.0f, 0.0f, 0.0f } } };

    const auto size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(eyePosition, At, Up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.01f, 100.0f);
#else
    XMMATRIX view = XMMatrixLookAtRH(eyePosition, At, Up);
    XMMATRIX projection = XMMatrixPerspectiveFovRH(XM_PIDIV4, aspect, 0.01f, 100.0f);
#endif

    m_effect->SetView(view);
    m_effect->SetProjection(projection);
}

#pragma endregion

void Game::UnitTests(bool success)
{
    auto device = m_deviceResources->GetD3DDevice();

    using namespace Xbox;

    // Alpha mode test
    {
        DDS_ALPHA_MODE alphaMode;
        void* grfxMemory = nullptr;

        ComPtr<ID3D12Resource> tree;
        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, c_TREE_PMALPHA,
            tree.ReleaseAndGetAddressOf(), &grfxMemory, &alphaMode));

        if (alphaMode != DDS_ALPHA_MODE_PREMULTIPLIED)
        {
            OutputDebugStringA("FAILED: tree02S_pmalpha.dds alpha mode unexpected\n");
            success = false;
        }

        const auto desc = tree->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 304
            || desc.Height != 268
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: tree02S_pmalpha.dds desc unexpected\n");
            success = false;
        }

        // Test memory free function
        tree.Reset();
        FreeDDSTextureMemory(grfxMemory);
    }

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");
}
