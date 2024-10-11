//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK MSAATest
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
#ifdef GAMMA_CORRECT_RENDERING
    // Linear colors for DirectXMath were not added until v3.17 in the Windows SDK (22621)
    const XMVECTORF32 c_clearColor = { { { 0.015996292f, 0.258182913f, 0.015996292f, 1.f } } };
    const XMVECTORF32 c_clearColor2 = { { { 0.417885154f, 0.686685443f, 0.791298151f, 1.f } } };
    const XMVECTORF32 c_clearColor4 = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
    const XMVECTORF32 c_clearColor8 = { { { 0.009721218f, 0.009721218f, 0.162029430f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::ForestGreen;
    const XMVECTORF32 c_clearColor2 = Colors::LightBlue;
    const XMVECTORF32 c_clearColor4 = Colors::CornflowerBlue;
    const XMVECTORF32 c_clearColor8 = Colors::MidnightBlue;
#endif

    constexpr float ADVANCE_TIME = 1.f;
    constexpr float INTERACTIVE_TIME = 10.f;
}

// Constructor.
Game::Game() noexcept(false) :
    m_state(State::MSAA4X),
    m_delay(0),
    m_frame(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

    // If you weren't doing 'switchable' MSAA, then you'd use DXGI_FORMAT_UNKNOWN because you never need the non-MSAA depth buffer
    constexpr DXGI_FORMAT c_DepthFormat = DXGI_FORMAT_D32_FLOAT;

#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, c_DepthFormat, 2,
        DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, c_DepthFormat, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
#endif

#ifdef _GAMING_XBOX
    m_deviceResources->SetClearColor(c_clearColor);
#endif

#ifdef LOSTDEVICE
    m_deviceResources->RegisterDeviceNotify(this);
#endif

    // Set up for MSAA rendering.
    m_msaaHelper2 = std::make_unique<DX::MSAAHelper>(m_deviceResources->GetBackBufferFormat(),
        c_DepthFormat,
        2);
    m_msaaHelper2->SetClearColor(c_clearColor2);

    m_msaaHelper4 = std::make_unique<DX::MSAAHelper>(m_deviceResources->GetBackBufferFormat(),
        c_DepthFormat,
        4);
    m_msaaHelper4->SetClearColor(c_clearColor4);

    m_msaaHelper8 = std::make_unique<DX::MSAAHelper>(m_deviceResources->GetBackBufferFormat(),
        c_DepthFormat,
        8);
    m_msaaHelper8->SetClearColor(c_clearColor8);

    m_delay = ADVANCE_TIME;
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
#ifdef COREWINDOW
    IUnknown* window,
#else
    HWND window,
#endif
    int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();

#ifdef XBOX
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
#ifdef COREWINDOW
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#endif
#elif defined(UWP)
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
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

#ifdef _GAMING_XBOX
    m_deviceResources->WaitForOrigin();
#endif

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitGame();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED
            || m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
        {
            m_delay = INTERACTIVE_TIME;

            ++m_state;
            if (m_state >= State::COUNT)
                m_state = State::NOMSAA;
        }
        else if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED
            || m_gamePadButtons.dpadLeft == GamePad::ButtonStateTracker::PRESSED)
        {
            m_delay = INTERACTIVE_TIME;

            --m_state;
            if (m_state < 0)
                m_state = State::COUNT - 1;
        }
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitGame();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        m_delay = INTERACTIVE_TIME;

        ++m_state;
        if (m_state >= State::COUNT)
            m_state = State::NOMSAA;
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::Back))
    {
        m_delay = INTERACTIVE_TIME;

        --m_state;
        if (m_state < 0)
            m_state = State::COUNT - 1;
    }

    m_delay -= static_cast<float>(timer.GetElapsedSeconds());

    if (m_delay <= 0.f)
    {
        m_delay = ADVANCE_TIME;

        ++m_state;
        if (m_state >= State::COUNT)
            m_state = State::NOMSAA;
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
    m_deviceResources->Prepare((m_state == State::NOMSAA) ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_RENDER_TARGET);
        // Note for MSAA, this doesn't actully put the backbuffer into RT state; it just assumes it's already in it and thus skips the barrier

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    switch (m_state)
    {
    case State::MSAA2X:
        m_msaaHelper2->Prepare(commandList);
        break;

    case State::MSAA4X:
        m_msaaHelper4->Prepare(commandList);
        break;

    case State::MSAA8X:
        m_msaaHelper8->Prepare(commandList);
        break;

    default:
        break;
    }

    Clear();

    // TODO: Add your rendering code here.

    unsigned int sampleCount = 1;

    switch (m_state)
    {
    case State::MSAA2X:
        m_msaaHelper2->Resolve(commandList, m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        sampleCount = m_msaaHelper2->GetSampleCount();
        break;

    case State::MSAA4X:
        m_msaaHelper4->Resolve(commandList, m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        sampleCount = m_msaaHelper4->GetSampleCount();
        if (m_frame == 10)
        {
            // TOOD: wrong state until I render?
            // m_screenshot = m_msaaHelper4->GetMSAARenderTarget();
        }
        break;

    case State::MSAA8X:
        m_msaaHelper8->Resolve(commandList, m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        sampleCount = m_msaaHelper8->GetSampleCount();
        break;

    default:
        break;
    }

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    // Sample stats to update peak values
    std::ignore = m_graphicsMemory->GetStatistics();

    PIXEndEvent(m_deviceResources->GetCommandQueue());

    if (m_screenshot)
    {
        // We take the shot here to cope with lost device

        OutputDebugStringA("****** MSAA SCREENSHOT TEST BEGIN **********\n");

        bool success = true;

    #if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
        const wchar_t sspath[MAX_PATH] = L"T:\\";
    #else
        wchar_t sspath[MAX_PATH];
        ExpandEnvironmentStringsW(L"%TEMP%\\", sspath, MAX_PATH);
    #endif

        OutputDebugStringA("Output path: ");
        OutputDebugStringW(sspath);
        OutputDebugStringA("\n");

        wchar_t sspng[MAX_PATH] = {};
        wchar_t ssjpg[MAX_PATH] = {};
        wchar_t ssbmp[MAX_PATH] = {};
        wchar_t sstif[MAX_PATH] = {};
        wchar_t ssdds[MAX_PATH] = {};
        wchar_t ssdds2[MAX_PATH] = {};

        swprintf_s(sspng, L"%ls\\SCREENSHOT.PNG", sspath);
        swprintf_s(ssjpg, L"%ls\\SCREENSHOT.JPG", sspath);
        swprintf_s(ssbmp, L"%ls\\SCREENSHOT.BMP", sspath);
        swprintf_s(sstif, L"%ls\\SCREENSHOT.TIF", sspath);
        swprintf_s(ssdds, L"%ls\\SCREENSHOT.DDS", sspath);
        swprintf_s(ssdds2, L"%ls\\SCREENSHOT2.DDS", sspath);

        std::ignore = DeleteFileW(sspng);
        std::ignore = DeleteFileW(ssjpg);
        std::ignore = DeleteFileW(ssbmp);
        std::ignore = DeleteFileW(sstif);
        std::ignore = DeleteFileW(ssdds);
        std::ignore = DeleteFileW(ssdds2);

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatPng, sspng,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET));

        if (GetFileAttributesW(sspng) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.PNG\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.PNG!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatJpeg, ssjpg,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET));

        if (GetFileAttributesW(ssjpg) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.JPG\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.JPG!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatBmp, ssbmp,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
            &GUID_WICPixelFormat16bppBGR565));

        if (GetFileAttributesW(ssbmp) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.BMP\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.BMP!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatTiff, sstif,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr,
            [&](IPropertyBag2* props)
            {
                PROPBAG2 options[2] = { 0, 0 };
                options[0].pstrName = const_cast<LPWSTR>(L"CompressionQuality");
                options[1].pstrName = const_cast<LPWSTR>(L"TiffCompressionMethod");

                VARIANT varValues[2];
                varValues[0].vt = VT_R4;
                varValues[0].fltVal = 0.75f;

                varValues[1].vt = VT_UI1;
                varValues[1].bVal = WICTiffCompressionNone;

                std::ignore = props->Write(2, options, varValues);
            }));

        if (GetFileAttributesW(sstif) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.TIF\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.TIF!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveDDSTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(), ssdds,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET));

        if (GetFileAttributesW(ssdds) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.DDS\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.DDS!\n");
            success = false;
        }

        OutputDebugStringA(success ? "Passed\n" : "Failed\n");
        OutputDebugStringA("******* MSAA SCREENSHOT TEST END ***********\n");
        m_screenshot.Reset();
    }
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    XMVECTORF32 color;
    switch (m_state)
    {
    case State::MSAA2X:
        rtvDescriptor = m_msaaHelper2->GetMSAARenderTargetView();
        dsvDescriptor = m_msaaHelper2->GetMSAADepthStencilView();
        color.v = c_clearColor2.v;
        break;

    case State::MSAA4X:
        rtvDescriptor = m_msaaHelper4->GetMSAARenderTargetView();
        dsvDescriptor = m_msaaHelper4->GetMSAADepthStencilView();
        color.v = c_clearColor4.v;
        break;

    case State::MSAA8X:
        rtvDescriptor = m_msaaHelper8->GetMSAARenderTargetView();
        dsvDescriptor = m_msaaHelper8->GetMSAADepthStencilView();
        color.v = c_clearColor8.v;
        break;

    default:
        color.v = c_clearColor.v;
        break;
    }

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, color, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto const viewport = m_deviceResources->GetScreenViewport();
    auto const scissorRect = m_deviceResources->GetScissorRect();
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

#ifdef PC
void Game::OnWindowMoved()
{
    auto const r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}
#endif

#if defined(PC) || defined(UWP)
void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}
#endif

#ifndef XBOX
void Game::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
#ifdef UWP
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

#ifdef UWP
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

    // Set the MSAA device. Note this updates updates GetSampleCount.
    m_msaaHelper2->SetDevice(device);
    m_msaaHelper4->SetDevice(device);
    m_msaaHelper8->SetDevice(device);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();

    // Set windows size for MSAA.
    m_msaaHelper2->SetWindow(size);
    m_msaaHelper4->SetWindow(size);
    m_msaaHelper8->SetWindow(size);

    //m_batch->SetViewport(m_deviceResources->GetScreenViewport());
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_graphicsMemory.reset();

    // Release MSAA resources.
    m_msaaHelper2->ReleaseDevice();
    m_msaaHelper4->ReleaseDevice();
    m_msaaHelper8->ReleaseDevice();

    m_screenshot.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion
