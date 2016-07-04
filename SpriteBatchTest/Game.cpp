//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

namespace
{
    inline float randf()
    {
        return (float)rand() / (float)RAND_MAX * 10000;
    }
}

using namespace DirectX;

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

    if (kb.Left)
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_ROTATE270);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_ROTATE270);
    }
    else if (kb.Right)
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_ROTATE90);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_ROTATE90);
    }
    else if (kb.Up)
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_IDENTITY);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_IDENTITY);
    }
    else if (kb.Down)
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_ROTATE180);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_ROTATE180);
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

    m_spriteBatch->Begin(commandList);

    float time = 60.f * static_cast<float>(m_timer.GetTotalSeconds());

    // Moving
    m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat), GetTextureSize(m_cat.Get()),
        XMFLOAT2(900, 384.f + sinf(time / 60.f)*384.f), nullptr, Colors::White, 0.f, XMFLOAT2(128, 128), 1, SpriteEffects_None, 0);

    // Spinning.
    m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat), GetTextureSize(m_cat.Get()), 
        XMFLOAT2(200, 150), nullptr, Colors::White, time / 100, XMFLOAT2(128, 128), 1, SpriteEffects_None, 0);

    // Zero size source region.
    RECT src = { 128, 128, 128, 140 };
    RECT dest = { 400, 150, 450, 200 };

    auto cat = m_resourceDescriptors->GetGpuHandle(Descriptors::Cat);
    auto catSize = GetTextureSize(m_cat.Get());

    m_spriteBatch->Draw(cat, catSize, dest, &src, Colors::White, time / 100, XMFLOAT2(0, 6), SpriteEffects_None, 0);

    // Differently scaled.
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(0, 0), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 0.5);

    RECT dest1 = { 0, 0, 256, 64 };
    RECT dest2 = { 0, 0, 64, 256 };

    m_spriteBatch->Draw(cat, catSize, dest1);
    m_spriteBatch->Draw(cat, catSize, dest2);

    // Mirroring.
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(300, 10), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 0.3f, SpriteEffects_None);
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(350, 10), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 0.3f, SpriteEffects_FlipHorizontally);
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(400, 10), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 0.3f, SpriteEffects_FlipVertically);
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(450, 10), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 0.3f, SpriteEffects_FlipBoth);

    // Sorting.
    auto letterA = m_resourceDescriptors->GetGpuHandle(Descriptors::A);
    auto letterASize = GetTextureSize(m_letterA.Get());

    auto letterB = m_resourceDescriptors->GetGpuHandle(Descriptors::B);
    auto letterBSize = GetTextureSize(m_letterB.Get());

    auto letterC = m_resourceDescriptors->GetGpuHandle(Descriptors::C);
    auto letterCSize = GetTextureSize(m_letterC.Get());

    m_spriteBatch->Draw(letterA, letterASize, XMFLOAT2(10, 280), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 1, SpriteEffects_None, 0.1f);
    m_spriteBatch->Draw(letterC, letterCSize, XMFLOAT2(15, 290), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 1, SpriteEffects_None, 0.9f);
    m_spriteBatch->Draw(letterB, letterBSize, XMFLOAT2(15, 285), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 1, SpriteEffects_None, 0.5f);

    m_spriteBatch->Draw(letterA, letterASize, XMFLOAT2(50, 280), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 1, SpriteEffects_None, 0.9f);
    m_spriteBatch->Draw(letterC, letterCSize, XMFLOAT2(55, 290), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 1, SpriteEffects_None, 0.1f);
    m_spriteBatch->Draw(letterB, letterBSize, XMFLOAT2(55, 285), nullptr, Colors::White, 0, XMFLOAT2(0, 0), 1, SpriteEffects_None, 0.5f);

    RECT source = { 16, 32, 256, 192 };

    // Draw overloads specifying position, origin and scale as XMFLOAT2.
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(-40, 320), Colors::Red);

    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(200, 320), nullptr, Colors::Lime, time / 500, XMFLOAT2(32, 128), 0.5f, SpriteEffects_None, 0.5f);
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(300, 320), &source, Colors::Lime, time / 500, XMFLOAT2(120, 80), 0.5f, SpriteEffects_None, 0.5f);

    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(350, 320), nullptr, Colors::Blue, time / 500, XMFLOAT2(32, 128), XMFLOAT2(0.25f, 0.5f), SpriteEffects_None, 0.5f);
    m_spriteBatch->Draw(cat, catSize, XMFLOAT2(450, 320), &source, Colors::Blue, time / 500, XMFLOAT2(120, 80), XMFLOAT2(0.5f, 0.25f), SpriteEffects_None, 0.5f);

    // Draw overloads specifying position, origin and scale via the first two components of an XMVECTOR.
    m_spriteBatch->Draw(cat, catSize, XMVectorSet(0, 450, randf(), randf()), Colors::Pink);

    m_spriteBatch->Draw(cat, catSize, XMVectorSet(200, 450, randf(), randf()), nullptr, Colors::Lime, time / 500, XMVectorSet(32, 128, randf(), randf()), 0.5f, SpriteEffects_None, 0.5f);
    m_spriteBatch->Draw(cat, catSize, XMVectorSet(300, 450, randf(), randf()), &source, Colors::Lime, time / 500, XMVectorSet(120, 80, randf(), randf()), 0.5f, SpriteEffects_None, 0.5f);

    m_spriteBatch->Draw(cat, catSize, XMVectorSet(350, 450, randf(), randf()), nullptr, Colors::Blue, time / 500, XMVectorSet(32, 128, randf(), randf()), XMVectorSet(0.25f, 0.5f, randf(), randf()), SpriteEffects_None, 0.5f);
    m_spriteBatch->Draw(cat, catSize, XMVectorSet(450, 450, randf(), randf()), &source, Colors::Blue, time / 500, XMVectorSet(120, 80, randf(), randf()), XMVectorSet(0.5f, 0.25f, randf(), randf()), SpriteEffects_None, 0.5f);

    // Draw overloads specifying position as a RECT.
    RECT rc1 = { 500, 320, 600, 420 };
    RECT rc2 = { 550, 450, 650, 550 };
    RECT rc3 = { 550, 550, 650, 650 };

    m_spriteBatch->Draw(cat, catSize, rc1, Colors::Gray);
    m_spriteBatch->Draw(cat, catSize, rc2, nullptr, Colors::LightSeaGreen, time / 300, XMFLOAT2(128, 128), SpriteEffects_None, 0.5f);
    m_spriteBatch->Draw(cat, catSize, rc3, &source, Colors::LightSeaGreen, time / 300, XMFLOAT2(128, 128), SpriteEffects_None, 0.5f);

    m_spriteBatch->End();

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
    width = 1024;
    height = 768;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        SpriteBatchPipelineStateDescription pd(
            &rtState,
            &CommonStates::NonPremultiplied);

        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, &pd);
    }

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"cat.dds",m_cat.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_cat.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cat));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"a.dds", m_letterA.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_letterA.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::A));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"b.dds", m_letterB.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_letterB.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::B));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"c.dds", m_letterC.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_letterC.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::C));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    m_deviceResources->WaitForGpu();

    uploadResourcesFinished.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);
}

void Game::OnDeviceLost()
{
    m_cat.Reset();
    m_letterA.Reset();
    m_letterB.Reset();
    m_letterC.Reset();
    m_resourceDescriptors.reset();
    m_spriteBatch.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
