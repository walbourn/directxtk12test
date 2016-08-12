//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

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

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    // SimpleMath interop tests for Windows Runtime types
    Rectangle test1(10, 20, 50, 100);

    Windows::Foundation::Rect test2 = test1;
    if (test1.x != long(test2.X)
        && test1.y != long(test2.Y)
        && test1.width != long(test2.Width)
        && test1.height != long(test2.Height))
    {
        OutputDebugStringA("SimpleMath::Rectangle operator test A failed!");
        throw ref new Platform::Exception(E_FAIL);
    }
#endif
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

    // Point
    m_effectPoint->Apply(commandList);

    m_batch->Begin(commandList);

    {
        VertexPositionColor points[]
        {
            { Vector3(-0.75f, -0.75f, 0.5f), Colors::Red },
            { Vector3(-0.75f, -0.5f,  0.5f), Colors::Green },
            { Vector3(-0.75f, -0.25f, 0.5f), Colors::Blue },
            { Vector3(-0.75f,  0.0f,  0.5f), Colors::Yellow },
            { Vector3(-0.75f,  0.25f, 0.5f), Colors::Magenta },
            { Vector3(-0.75f,  0.5f,  0.5f), Colors::Cyan },
            { Vector3(-0.75f,  0.75f, 0.5f), Colors::White },
        };

        m_batch->Draw(D3D_PRIMITIVE_TOPOLOGY_POINTLIST, points, _countof(points));
    }

    m_batch->End();

    // Lines
    m_effectLine->Apply(commandList);

    m_batch->Begin(commandList);

    {
        VertexPositionColor lines[] =
        {
            { Vector3(-0.75f, -0.85f, 0.5f), Colors::Red },{ Vector3(0.75f, -0.85f, 0.5f), Colors::DarkRed },
            { Vector3(-0.75f, -0.90f, 0.5f), Colors::Green },{ Vector3(0.75f, -0.90f, 0.5f), Colors::DarkGreen },
            { Vector3(-0.75f, -0.95f, 0.5f), Colors::Blue },{ Vector3(0.75f, -0.95f, 0.5f), Colors::DarkBlue },
        };

        m_batch->DrawLine(lines[0], lines[1]);
        m_batch->DrawLine(lines[2], lines[3]);
        m_batch->DrawLine(lines[4], lines[5]);
    }

    m_batch->End();

    // Triangle
    m_effectTri->Apply(commandList);

    m_batch->Begin(commandList);

    {
        VertexPositionColor v1(Vector3(0.f, 0.5f, 0.5f), Colors::Red);
        VertexPositionColor v2(Vector3(0.5f, -0.5f, 0.5f), Colors::Green);
        VertexPositionColor v3(Vector3(-0.5f, -0.5f, 0.5f), Colors::Blue);

        m_batch->DrawTriangle(v1, v2, v3);
    }

    // Quad (same type as triangle)
    {
        VertexPositionColor quad[] =
        {
            { Vector3(0.75f, 0.75f, 0.5), Colors::Gray },
            { Vector3(0.95f, 0.75f, 0.5), Colors::Gray },
            { Vector3(0.95f, -0.75f, 0.5), Colors::DarkGray },
            { Vector3(0.75f, -0.75f, 0.5), Colors::DarkGray },
        };

        m_batch->DrawQuad(quad[0], quad[1], quad[2], quad[3]);
    }

    m_batch->End();

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

    m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        m_effectTri = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);

        pd.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        m_effectPoint = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);

        pd.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        m_effectLine = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    SetDebugObjectName(m_deviceResources->GetRenderTarget(), L"BackBuffer");

    SetDebugObjectName(m_deviceResources->GetDepthStencil(), L"DepthStencil");
}

void Game::OnDeviceLost()
{
    m_batch.reset();
    m_effectTri.reset();
    m_effectPoint.reset();
    m_effectLine.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
