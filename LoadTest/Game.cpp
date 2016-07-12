//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

// Build for LH vs. RH coords
//#define LH_COORDS

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

Game::Game()
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    m_deviceResources->WaitForGpu();
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_keyboard = std::make_unique<Keyboard>();

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    elapsedTime;

    auto kb = m_keyboard->GetState();

    if (kb.Escape)
    {
        PostQuitMessage(0);
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
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(1, heaps);

    float dist = 10.f;

    auto t = static_cast<float>(m_timer.GetTotalSeconds());

    // Cube 1
    XMMATRIX world = XMMatrixRotationY(t) * XMMatrixTranslation(1.5f, -2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Earth));
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 2
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(1.5f, 0, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Windows95_sRGB));
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 3
    world = XMMatrixRotationY(t) * XMMatrixTranslation(1.5f, 2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Windows95));
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 4
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(-1.5f, -2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Earth_sRGB));
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 5
    world = XMMatrixRotationY(t) * XMMatrixTranslation(-1.5f, 0, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo_BC1));
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 6
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(-1.5f, 2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo));
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
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
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

#ifdef LH_COORDS
    m_cube = GeometricPrimitive::CreateCube(2.f, false);
#else
    m_cube = GeometricPrimitive::CreateCube(2.f, true);
#endif

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Texture, pd);
    }

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    // View textures
    bool success = true;
    OutputDebugStringA("*********** UINT TESTS BEGIN ***************\n");

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    // Earth
    DDS_ALPHA_MODE alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"earth_A2B10G10R10.dds", m_earth.ReleaseAndGetAddressOf(), false, 0, &alphaMode));

    {
        if (alphaMode != DDS_ALPHA_MODE_UNKNOWN)
        {
            OutputDebugStringA("FAILED: earth_A2B10G10R10.dds alpha mode value unexpected\n");
            success = false;
        }

        auto desc = m_earth->GetDesc();
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

    CreateShaderResourceView(device, m_earth.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth));

    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"earth_A2B10G10R10.dds", 0, D3D12_RESOURCE_FLAG_NONE, true, false, m_earth2.ReleaseAndGetAddressOf()));

    {
        auto desc = m_earth2->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM
            || desc.Width != 512
            || desc.Height != 256
            || desc.MipLevels != 10)
        {
            OutputDebugStringA("FAILED: earth_A2B10G10R10.dds (sRGB) desc unexpected\n");
            success = false;
        }
    }

    CreateShaderResourceView(device, m_earth.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth_sRGB));

    // DirectX Logo
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen.dds", m_test1.ReleaseAndGetAddressOf(), false));
    {
        auto desc = m_test1->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: dx5_logo_autogen.dds (no autogen) desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen.dds", m_dxlogo.ReleaseAndGetAddressOf(), true));

    {
        auto desc = m_dxlogo->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: dx5_logo_autogen.dds (autogen) desc unexpected\n");
            success = false;
        }
    }

    CreateShaderResourceView(device, m_dxlogo.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo));

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo.dds", m_test2.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test2->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_BC1_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: dx5_logo.dds (BC1) desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"dx5_logo.dds", 0, D3D12_RESOURCE_FLAG_NONE, true, false, m_dxlogo2.ReleaseAndGetAddressOf()));

    {
        auto desc = m_dxlogo2->GetDesc();
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

    CreateShaderResourceView(device, m_dxlogo2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo_BC1));
    
    // Windows 95 logo
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"win95.bmp", m_test3.ReleaseAndGetAddressOf(), false));

    {
        auto desc = m_test3->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: win95.bmp desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"win95.bmp", m_win95.ReleaseAndGetAddressOf(), true));

    {
        auto desc = m_win95->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: win95.bmp (autogen) desc unexpected\n");
            success = false;
        }
    }

    CreateShaderResourceView(device, m_win95.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Windows95));

    DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"win95.bmp", 0, D3D12_RESOURCE_FLAG_NONE, true, true, m_win95_2.ReleaseAndGetAddressOf()));

    {
        auto desc = m_win95->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: win95.bmp (sRGB) desc unexpected\n");
            success = false;
        }
    }

    CreateShaderResourceView(device, m_win95_2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Windows95_sRGB));

    UnitTests(resourceUpload, success);

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    m_deviceResources->WaitForGpu();

    uploadResourcesFinished.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 eyePosition = { 0.0f, 3.0f, -6.0f, 0.0f };
    static const XMVECTORF32 At = { 0.0f, 1.0f, 0.0f, 0.0f };
    static const XMVECTORF32 Up = { 0.0f, 1.0f, 0.0f, 0.0f };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

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

void Game::OnDeviceLost()
{
    m_earth.Reset();
    m_earth2.Reset();
    m_dxlogo.Reset();
    m_dxlogo2.Reset();
    m_win95.Reset();
    m_win95_2.Reset();

    m_test1.Reset();
    m_test2.Reset();
    m_test3.Reset();
    m_test4.Reset();
    m_test5.Reset();
    m_test6.Reset();
    m_test7.Reset();
    m_test8.Reset();
    m_test9.Reset();
    m_test10.Reset();

    m_cube.reset();
    m_effect.reset();
    m_resourceDescriptors.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

void Game::UnitTests(ResourceUploadBatch& resourceUpload, bool success)
{
    auto device = m_deviceResources->GetD3DDevice();

    // Alpha mode test
    DDS_ALPHA_MODE alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"tree02S_pmalpha.dds", m_test4.ReleaseAndGetAddressOf(), false, 0, &alphaMode));
    
    {
        if (alphaMode != DDS_ALPHA_MODE_PREMULTIPLIED)
        {
            OutputDebugStringA("FAILED: tree02S_pmalpha.dds alpha mode unexpected\n");
            success = false;
        }

        auto desc = m_test4->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 304
            || desc.Height != 268
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: tree02S_pmalpha.dds desc unexpected\n");
            success = false;
        }
    }

    // 1D texture
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1D_MipOff.dds", m_test5.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test5->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1D_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // 1D texture array
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1DArray_MipOff.dds", m_test6.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test6->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.MipLevels != 1
            || desc.DepthOrArraySize != 6)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1DArray_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // 2D texture array
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE2DArray_MipOff.dds", m_test7.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test7->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.Height != 128
            || desc.MipLevels != 1
            || desc.DepthOrArraySize != 6)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE2DArray_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // 3D texture
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE3D_MipOff.dds", m_test8.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test8->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.Height != 128
            || desc.MipLevels != 1
            || desc.DepthOrArraySize != 32)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE3D_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // DX12 version doesn't support auto-gen mips for 1D, 1D arrays, 2D arrays, or 3D. They throw an exception instead of being ignored like on DX11

    // sRGB test
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"cup_small.jpg", m_test9.ReleaseAndGetAddressOf(), false));

    {
        auto desc = m_test9->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 512
            || desc.Height != 683
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"cup_small.jpg", m_test10.ReleaseAndGetAddressOf(), true));

    {
        auto desc = m_test9->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 512
            || desc.Height != 683
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg desc unexpected\n");
            success = false;
        }
    }

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");
}
