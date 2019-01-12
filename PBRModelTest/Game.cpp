//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK ?
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

// Build for LH vs. RH coords
//#define LH_COORDS

// For UWP/PC, this tests using a linear F16 swapchain intead of HDR10
//#define TEST_HDR_LINEAR

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

// Constructor.
Game::Game() noexcept(false)
{
#ifdef TEST_HDR_LINEAR
    const DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
#else
    const DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
#endif

#if defined(_XBOX_ONE) && defined(_TITLE)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD | DX::DeviceResources::c_EnableHDR);
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_EnableHDR | DX::DeviceResources::c_Enable4K_Xbox);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_EnableHDR);
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_deviceResources->RegisterDeviceNotify(this);
#endif

    // Set up for HDR rendering.
    m_hdrScene = std::make_unique<DX::RenderTexture>(DXGI_FORMAT_R16G16B16A16_FLOAT);

    XMVECTORF32 color;
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
    m_hdrScene->SetClearColor(color);
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
    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();

#if defined(_XBOX_ONE) && defined(_TITLE)
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
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
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    elapsedTime;

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

    auto commandList = m_deviceResources->GetCommandList();
    m_hdrScene->BeginScene(commandList);

    Clear();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // TODO: Add your rendering code here.

    PIXEndEvent(commandList);

    // Tonemap the frame.
    m_hdrScene->EndScene(commandList);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Tonemap");

#if defined(_XBOX_ONE) && defined(_TITLE)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptors[2] = { m_deviceResources->GetRenderTargetView(), m_deviceResources->GetGameDVRRenderTargetView() };
    commandList->OMSetRenderTargets(2, rtvDescriptors, FALSE, nullptr);

    m_toneMap->Process(commandList);
#else
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

    switch (m_deviceResources->GetColorSpace())
    {
    default:
        m_toneMap->Process(commandList);
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        m_toneMapHDR10->Process(commandList);
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        m_toneMapLinear->Process(commandList);
        break;
    }
#endif

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
    auto rtvDescriptor = m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene);
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    XMVECTORF32 color;
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, color, 0, nullptr);
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
#if defined(_XBOX_ONE) && defined(_TITLE)
    auto queue = m_deviceResources->GetCommandQueue();
    queue->SuspendX(0);
#endif
}

void Game::OnResuming()
{
#if defined(_XBOX_ONE) && defined(_TITLE)
    auto queue = m_deviceResources->GetCommandQueue();
    queue->ResumeX();
#endif

    m_timer.ResetElapsedTime();
}

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
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
#endif

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
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_hdrScene->SetDevice(device,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene));

    RenderTargetState hdrState(m_hdrScene->GetFormat(), m_deviceResources->GetDepthBufferFormat());
    // TODO -

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
#if defined(_XBOX_ONE) && defined(_TITLE)
    rtState.numRenderTargets = 2;
    rtState.rtvFormats[1] = m_deviceResources->GetGameDVRFormat();
#endif

    m_toneMap = std::make_unique<ToneMapPostProcess>(
        device,
        rtState,
        ToneMapPostProcess::ACESFilmic, (m_deviceResources->GetBackBufferFormat() == DXGI_FORMAT_R16G16B16A16_FLOAT) ? ToneMapPostProcess::Linear : ToneMapPostProcess::SRGB
#if defined(_XBOX_ONE) && defined(_TITLE)
        , true
#endif
        );

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::Linear);
    m_toneMapHDR10 = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::ST2084);
#endif
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();

    // TODO: Initialize windows-size dependent objects here.

    // Set windows size for HDR.
    m_hdrScene->SetWindow(size);

    m_toneMap->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
    m_toneMapHDR10->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
#endif
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.

    m_toneMap.reset();

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear.reset();
    m_toneMapHDR10.reset();
#endif

    m_hdrScene->ReleaseDevice();

    m_resourceDescriptors.reset();
    m_renderDescriptors.reset();

    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion
