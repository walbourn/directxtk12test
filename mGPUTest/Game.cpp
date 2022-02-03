//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for mGPU Direct3D 12 support
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#define GAMMA_CORRECT_RENDERING

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

// Constructor.
Game::Game() noexcept(false)
{
#ifdef GAMMA_CORRECT_RENDERING
    DXGI_FORMAT s_format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    DXGI_FORMAT s_format = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

    m_deviceResources = std::make_unique<DX::DeviceResources>(s_format, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0, 0, 2);

    m_deviceResources->RegisterDeviceNotify(this);
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
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
    HWND window,
#else
    IUnknown* window,
#endif
    int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad	= std::make_unique<GamePad>();
    m_keyboard	= std::make_unique<Keyboard>();

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_deviceResources->SetWindow(window, width, height, rotation);
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

#else
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
#endif

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
void Game::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad		= m_gamePad->GetState(0);
    auto kb			= m_keyboard->GetState();

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

    XMVECTORF32 red, green, blue, dred, dgreen, dblue, yellow, cyan, magenta, gray, dgray;
#ifdef GAMMA_CORRECT_RENDERING
    red.v = XMColorSRGBToRGB(Colors::Red);
    green.v = XMColorSRGBToRGB(Colors::Green);
    blue.v = XMColorSRGBToRGB(Colors::Blue);
    dred.v = XMColorSRGBToRGB(Colors::DarkRed);
    dgreen.v = XMColorSRGBToRGB(Colors::DarkGreen);
    dblue.v = XMColorSRGBToRGB(Colors::DarkBlue);
    yellow.v = XMColorSRGBToRGB(Colors::Yellow);
    cyan.v = XMColorSRGBToRGB(Colors::Cyan);
    magenta.v = XMColorSRGBToRGB(Colors::Magenta);
    gray.v = XMColorSRGBToRGB(Colors::Gray);
    dgray.v = XMColorSRGBToRGB(Colors::DarkGray);
#else
    red.v = Colors::Red;
    green.v = Colors::Green;
    blue.v = Colors::Blue;
    dred.v = Colors::DarkRed;
    dgreen.v = Colors::DarkGreen;
    dblue.v = Colors::DarkBlue;
    yellow.v = Colors::Yellow;
    cyan.v = Colors::Cyan;
    magenta.v = Colors::Magenta;
    gray.v = Colors::Gray;
    dgray.v = Colors::DarkGray;
#endif

    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceResources->GetDeviceCount(); ++adapterIdx)
    {
        auto commandList = m_deviceResources->GetCommandList(adapterIdx);
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

        // Point
        m_effectPoint[adapterIdx]->Apply(commandList);

        m_batch[adapterIdx]->Begin(commandList);

        {
            VertexPositionColor points[] = {{ Vector3(-0.75f, -0.75f, 0.5f), red },			{ Vector3(-0.75f, -0.5f,  0.5f), green },
                                            { Vector3(-0.75f, -0.25f, 0.5f), blue },		{ Vector3(-0.75f,  0.0f,  0.5f), yellow },
                                            { Vector3(-0.75f,  0.25f, 0.5f), magenta },		{ Vector3(-0.75f,  0.5f,  0.5f), cyan },
                                            { Vector3(-0.75f,  0.75f, 0.5f), Colors::White} };

            m_batch[adapterIdx]->Draw(D3D_PRIMITIVE_TOPOLOGY_POINTLIST, points, static_cast<UINT>(std::size(points)));
        }

        m_batch[adapterIdx]->End();

        // Lines
        m_effectLine[adapterIdx]->Apply(commandList);

        m_batch[adapterIdx]->Begin(commandList);

        {
            VertexPositionColor lines[] = { { Vector3(-0.75f, -0.85f, 0.5f), red },			{ Vector3(0.75f, -0.85f, 0.5f), dred },
                                            { Vector3(-0.75f, -0.90f, 0.5f), green },		{ Vector3(0.75f, -0.90f, 0.5f), dgreen },
                                            { Vector3(-0.75f, -0.95f, 0.5f), blue },		{ Vector3(0.75f, -0.95f, 0.5f), dblue }};

            m_batch[adapterIdx]->DrawLine(lines[0], lines[1]);
            m_batch[adapterIdx]->DrawLine(lines[2], lines[3]);
            m_batch[adapterIdx]->DrawLine(lines[4], lines[5]);
        }

        m_batch[adapterIdx]->End();

        // Triangle
        m_effectTri[adapterIdx]->Apply(commandList);

        m_batch[adapterIdx]->Begin(commandList);

        VertexPositionColor tri[]   = {{ Vector3(0.f, 0.5f, 0.5f), red},                    {Vector3(0.5f, -0.5f, 0.5f), green},
                                       { Vector3(-0.5f, -0.5f, 0.5f), blue}};
        
        m_batch[adapterIdx]->DrawTriangle(tri[0], tri[1], tri[2]);

        // Quad (same type as triangle)
        
        VertexPositionColor quad[]  = {{ Vector3(0.75f, 0.75f, 0.5), gray },				{ Vector3(0.95f, 0.75f, 0.5), gray },
                                       { Vector3(0.95f, -0.75f, 0.5), dgray },			    { Vector3(0.75f, -0.75f, 0.5), dgray }};

        m_batch[adapterIdx]->DrawQuad(quad[0], quad[1], quad[2], quad[3]);
    
        m_batch[adapterIdx]->End();

        PIXEndEvent(commandList);
    }

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(0), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceResources->GetDeviceCount(); ++adapterIdx)
    {
        m_graphicsMemory[adapterIdx]->Commit(m_deviceResources->GetCommandQueue(adapterIdx));
    }

    PIXEndEvent(m_deviceResources->GetCommandQueue(0));
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceResources->GetDeviceCount(); ++adapterIdx)
    {
        auto commandList = m_deviceResources->GetCommandList(adapterIdx);
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

        // Clear the views.
        auto rtvDescriptor = m_deviceResources->GetRenderTargetView(adapterIdx);
        auto dsvDescriptor = m_deviceResources->GetDepthStencilView(adapterIdx);

        XMVECTORF32 color;
#ifdef GAMMA_CORRECT_RENDERING
        color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
#else
        color.v = Colors::CornflowerBlue;
#endif
        commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
        commandList->ClearRenderTargetView(rtvDescriptor, color, 0, nullptr);
        commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // Set the viewport and scissor rect.
        auto viewport = m_deviceResources->GetScreenViewport();
        commandList->RSSetViewports(1, &viewport);

        auto scissorRect = m_deviceResources->GetScissorRect();
        commandList->RSSetScissorRects(1, &scissorRect);

        PIXEndEvent(commandList);
    }
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

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}
#endif

void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;
#else
    UNREFERENCED_PARAMETER(rotation);
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;
#endif

    CreateWindowSizeDependentResources();
}

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
void Game::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}
#endif

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
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    EffectPipelineStateDescription pd(&VertexPositionColor::InputLayout, CommonStates::Opaque, CommonStates::DepthDefault, CommonStates::CullNone, rtState);

    for (unsigned int adapterIdx = 0; adapterIdx < m_deviceResources->GetDeviceCount() && adapterIdx < MAX_DEVICES; ++adapterIdx)
    {
        auto device						= m_deviceResources->GetD3DDevice(adapterIdx);

        m_graphicsMemory[adapterIdx]	= std::make_unique<GraphicsMemory>(device);
        m_batch[adapterIdx]				= std::make_unique<PrimitiveBatch<VertexPositionColor>>(device);
       
        pd.primitiveTopology            = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        m_effectTri[adapterIdx]			= std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);

        pd.primitiveTopology			= D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        m_effectPoint[adapterIdx]		= std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);

        pd.primitiveTopology			= D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        m_effectLine[adapterIdx]		= std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceResources->GetDeviceCount(); ++adapterIdx)
    {
        SetDebugObjectName(m_deviceResources->GetRenderTarget(adapterIdx), L"BackBuffer");
        SetDebugObjectName(m_deviceResources->GetDepthStencil(adapterIdx), L"DepthStencil");
    }
}

void Game::OnDeviceLost()
{
    for (unsigned int adapterIdx = 0; adapterIdx != m_deviceResources->GetDeviceCount(); ++adapterIdx)
    {
        m_batch[adapterIdx].reset();
        m_effectTri[adapterIdx].reset();
        m_effectPoint[adapterIdx].reset();
        m_effectLine[adapterIdx].reset();
        m_graphicsMemory[adapterIdx].reset();
    }
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion
