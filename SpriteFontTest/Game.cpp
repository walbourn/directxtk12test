//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

namespace
{
    const float EPSILON = 0.000001f;
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

    UnitTests();
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

#if 0
    m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat),
        GetTextureSize(m_texture.Get()),
        m_screenPos, nullptr, Colors::White, 0.f, m_origin);
#endif

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
    width = 800;
    height = 600;
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
        SpriteBatchPipelineStateDescription pd(&rtState);

        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, &pd);
    }

    m_comicFont = std::make_unique<SpriteFont>(device, resourceUpload, L"comic.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ComicFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ComicFont));

    m_italicFont = std::make_unique<SpriteFont>(device, resourceUpload, L"italic.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ItalicFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ItalicFont));

    m_scriptFont = std::make_unique<SpriteFont>(device, resourceUpload, L"script.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ScriptFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ScriptFont));

    m_nonproportionalFont = std::make_unique<SpriteFont>(device, resourceUpload, L"multicolored.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::NonProportionalFont), m_resourceDescriptors->GetGpuHandle(Descriptors::NonProportionalFont));

    m_multicoloredFont = std::make_unique<SpriteFont>(device, resourceUpload, L"multicolored.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::MulticoloredFont), m_resourceDescriptors->GetGpuHandle(Descriptors::MulticoloredFont));

    m_japaneseFont = std::make_unique<SpriteFont>(device, resourceUpload, L"japanese.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::JapaneseFont), m_resourceDescriptors->GetGpuHandle(Descriptors::JapaneseFont));

    m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload, L"xboxController.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::CtrlFont), m_resourceDescriptors->GetGpuHandle(Descriptors::CtrlFont));

    m_consolasFont = std::make_unique<SpriteFont>(device, resourceUpload, L"consolas.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ConsolasFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ConsolasFont));

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
    m_comicFont.reset();
    m_italicFont.reset();
    m_scriptFont.reset();
    m_nonproportionalFont.reset();
    m_multicoloredFont.reset();
    m_japaneseFont.reset();
    m_ctrlFont.reset();
    m_consolasFont.reset();

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

void Game::UnitTests()
{
    OutputDebugStringA("*********** UNIT TESTS BEGIN ***************\n");

    // GetDefaultCharacterTest
    if (m_comicFont->GetDefaultCharacter() != 0)
    {
        OutputDebugStringA("FAILED: GetDefaultCharacter failed\n");
    }

    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");
}