//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Mouse
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include "FindMedia.h"

#define GAMMA_CORRECT_RENDERING

// Enable to test using always relative mode and not using absolute
//#define TEST_LOCKED_RELATIVE

// Remove call to Mouse::EndOfInputFrame by definining this.
//#define RELY_ON_AUTO_RESET

#ifdef USING_GAMEINPUT
#include <GameInput.h>
#ifndef GAMEINPUT_API_VERSION
#define GAMEINPUT_API_VERSION 0
#endif
#endif

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr XMVECTORF32 START_POSITION = { { { 0.f, -1.5f, 0.f, 0.f } } };
    constexpr XMVECTORF32 ROOM_BOUNDS = { { { 8.f, 6.f, 12.f, 0.f } } };

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif

}

static_assert(std::is_nothrow_move_constructible<Mouse>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<Mouse>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<Mouse::ButtonStateTracker>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<Mouse::ButtonStateTracker>::value, "Move Assign.");

Game::Game() noexcept(false) :
    m_ms{},
    m_lastMode(Mouse::MODE_ABSOLUTE),
    m_pitch(0),
    m_yaw(0),
    m_frame(0),
    m_lastStr(nullptr)
{
    m_cameraPos = START_POSITION.v;

#ifdef GAMMA_CORRECT_RENDERING
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

#ifdef COMBO_GDK
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
#elif defined(XBOX)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D24_UNORM_S8_UINT, 2, D3D_FEATURE_LEVEL_11_0,
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
    m_keyboard = std::make_unique<Keyboard>();

    m_mouse = std::make_unique<Mouse>();

#ifdef COMBO_GDK
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
    m_mouse->SetWindow(window);
#elif defined(XBOX)
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
#ifdef COREWINDOW
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
    m_mouse->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#endif
#elif defined(UWP)
    m_deviceResources->SetWindow(window, width, height, rotation);
    m_mouse->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#else
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
    m_mouse->SetWindow(window);
#endif

#ifdef TEST_LOCKED_RELATIVE
    m_mouse->SetMode(Mouse::MODE_RELATIVE);
#endif

#ifdef USING_COREWINDOW
    OutputDebugStringA("INFO: Using CoreWindow\n");
#elif defined(USING_GAMEINPUT)
    char buff[64] = {};
    sprintf_s(buff, "INFO: Using GameInput (API v%d)\n", GAMEINPUT_API_VERSION);
    OutputDebugStringA(buff);
#else
    OutputDebugStringA("INFO: Using Win32 messages\n");
#endif

    // Singleton test
    {
        bool thrown = false;

        try
        {
            std::unique_ptr<Mouse> mouse2(new Mouse);
        }
        catch (...)
        {
            thrown = true;
        }

        if (!thrown)
        {
#ifdef PC
            MessageBoxW(window, L"Mouse not acting like a singleton", L"MouseTest", MB_ICONERROR);
#else
            throw std::runtime_error("Mouse not acting like a singleton");
#endif
        }
    }

    OutputDebugStringA(m_mouse->IsConnected() ? "INFO: Mouse is connected\n" : "INFO: No mouse found\n");
    OutputDebugStringA(m_mouse->IsVisible() ? "INFO: Mouse cursor is visible on startup\n" : "INFO: Mouse cursor NOT visible on startup\n");

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

#ifndef RELY_ON_AUTO_RESET
    m_mouse->EndOfInputFrame();
#endif

    Render();

    PIXEndEvent();
    ++m_frame;
}

// Updates the world.
void Game::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitGame();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        const bool isvisible = m_mouse->IsVisible();
        m_mouse->SetVisible(!isvisible);

        OutputDebugStringA(m_mouse->IsVisible() ? "INFO: Mouse cursor is visible\n" : "INFO: Mouse cursor NOT visible\n");
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Home))
    {
        m_mouse->ResetScrollWheelValue();
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::End))
    {
#ifndef TEST_LOCKED_RELATIVE
        if (m_lastMode == Mouse::MODE_ABSOLUTE)
        {
            m_mouse->SetMode(Mouse::MODE_RELATIVE);
        }
        else
        {
            m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
        }
#endif
    }

    auto mouse = m_mouse->GetState();
    m_lastMode = mouse.positionMode;

    if (mouse.positionMode == Mouse::MODE_RELATIVE)
    {
        const SimpleMath::Vector4 ROTATION_GAIN(0.004f, 0.004f, 0.f, 0.f);
        const XMVECTOR delta = SimpleMath::Vector4(float(mouse.x), float(mouse.y), 0.f, 0.f) * ROTATION_GAIN;

        m_pitch -= XMVectorGetY(delta);
        m_yaw -= XMVectorGetX(delta);

        // limit pitch to straight up or straight down
        float limit = XM_PI / 2.0f - 0.01f;
        m_pitch = std::max(-limit, m_pitch);
        m_pitch = std::min(+limit, m_pitch);

        // keep longitude in sane range by wrapping
        if (m_yaw > XM_PI)
        {
            m_yaw -= XM_PI * 2.0f;
        }
        else if (m_yaw < -XM_PI)
        {
            m_yaw += XM_PI * 2.0f;
        }
    }

    m_tracker.Update(mouse);

    using ButtonState = Mouse::ButtonStateTracker::ButtonState;

    if (m_tracker.leftButton == ButtonState::PRESSED)
        m_lastStr = L"LeftButton was pressed";
    else if (m_tracker.leftButton == ButtonState::RELEASED)
        m_lastStr = L"LeftButton was released";
    else if (m_tracker.rightButton == ButtonState::PRESSED)
        m_lastStr = L"RightButton was pressed";
    else if (m_tracker.rightButton == ButtonState::RELEASED)
        m_lastStr = L"RightButton was released";
    else if (m_tracker.middleButton == ButtonState::PRESSED)
        m_lastStr = L"MiddleButton was pressed";
    else if (m_tracker.middleButton == ButtonState::RELEASED)
        m_lastStr = L"MiddleButton was released";
    else if (m_tracker.xButton1 == ButtonState::PRESSED)
        m_lastStr = L"XButton1 was pressed";
    else if (m_tracker.xButton1 == ButtonState::RELEASED)
        m_lastStr = L"XButton1 was released";
    else if (m_tracker.xButton2 == ButtonState::PRESSED)
        m_lastStr = L"XButton2 was pressed";
    else if (m_tracker.xButton2 == ButtonState::RELEASED)
        m_lastStr = L"XButton2 was released";

#ifndef TEST_LOCKED_RELATIVE
    if (m_tracker.leftButton == ButtonState::PRESSED)
    {
        m_mouse->SetMode(Mouse::MODE_RELATIVE);
    }
    else if (m_tracker.leftButton == ButtonState::RELEASED)
    {
        m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
    }
#endif

    m_ms = mouse;

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

    ID3D12DescriptorHeap* const heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    XMVECTORF32 red, blue, lightGray, yellow;
#ifdef GAMMA_CORRECT_RENDERING
    red.v = XMColorSRGBToRGB(Colors::Red);
    blue.v = XMColorSRGBToRGB(Colors::Blue);
    lightGray.v = XMColorSRGBToRGB(Colors::LightGray);
    yellow.v = XMColorSRGBToRGB(Colors::Yellow);
#else
    red.v = Colors::Red;
    blue.v = Colors::Blue;
    lightGray.v = Colors::LightGray;
    yellow.v = Colors::Yellow;
#endif

    const float y = sinf(m_pitch);      // vertical
    const float r = cosf(m_pitch);      // in the plane
    const float z = r * cosf(m_yaw);    // fwd-back
    const float x = r * sinf(m_yaw);    // left-right

    XMVECTOR lookAt = XMVectorAdd(m_cameraPos, XMVectorSet(x, y, z, 0.f));

    XMMATRIX view = XMMatrixLookAtRH(m_cameraPos, lookAt, Vector3::Up);

    m_roomEffect->SetView(view);
    m_roomEffect->Apply(commandList);
    m_room->Draw(commandList);

    const XMVECTOR xsize = m_comicFont->MeasureString(L"X");

    const float height = XMVectorGetY(xsize);

    m_spriteBatch->Begin(commandList);

    XMFLOAT2 pos(50, 50);

    // Buttons
    m_comicFont->DrawString(m_spriteBatch.get(), L"LeftButton", pos, m_ms.leftButton ? red : lightGray);
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"RightButton", pos, m_ms.rightButton ? red : lightGray);
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"MiddleButton", pos, m_ms.middleButton ? red : lightGray);
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"XButton1", pos, m_ms.xButton1 ? red : lightGray);
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"XButton2", pos, m_ms.xButton2 ? red : lightGray);

    // Scroll Wheel
    pos.y += height * 2;
    {
        wchar_t buff[16] = {};
        swprintf_s(buff, L"%d", m_ms.scrollWheelValue);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, Colors::Black);
    }

    m_comicFont->DrawString(m_spriteBatch.get(), (m_ms.positionMode == Mouse::MODE_RELATIVE) ? L"Relative" : L"Absolute",
        XMFLOAT2(50, 550), blue);

    if (m_lastStr)
    {
        m_comicFont->DrawString(m_spriteBatch.get(), m_lastStr, XMFLOAT2(50, 600), yellow);
    }

    if (m_ms.positionMode == Mouse::MODE_ABSOLUTE)
    {
        auto arrowSize = GetTextureSize(m_cursor.Get());
        m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::ArrowTex), arrowSize, XMFLOAT2((float)m_ms.x, (float)m_ms.y));
    }

    m_spriteBatch->End();

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    // Sample stats to update peak values
    std::ignore = m_graphicsMemory->GetStatistics();

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
void Game::OnActivated()
{
    m_tracker.Reset();
    m_keyboardButtons.Reset();
}

void Game::OnSuspending()
{
    m_deviceResources->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_tracker.Reset();
    m_keyboardButtons.Reset();
    m_timer.ResetElapsedTime();
}

#ifdef PC
void Game::OnWindowMoved()
{
    const auto r = m_deviceResources->GetOutputSize();
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

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

    m_states = std::make_unique<CommonStates>(device);

    m_room = GeometricPrimitive::CreateBox(XMFLOAT3(ROOM_BOUNDS[0], ROOM_BOUNDS[1], ROOM_BOUNDS[2]), false, true);

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        m_roomEffect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pd);
        m_roomEffect->EnableDefaultLighting();
    }

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    {
        const SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"comic.spritefont");
    m_comicFont = std::make_unique<SpriteFont>(device, resourceUpload,
        strFilePath,
        m_resourceDescriptors->GetCpuHandle(Descriptors::ComicFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::ComicFont));

#ifdef GAMMA_CORRECT_RENDERING
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_FORCE_SRGB;
#else
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_DEFAULT;
#endif

    DX::FindMediaFile(strFilePath, MAX_PATH, L"texture.dds");
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, strFilePath, 0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
        m_roomTex.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_roomTex.Get(),
        m_resourceDescriptors->GetCpuHandle(Descriptors::RoomTex));

    m_roomEffect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::RoomTex), m_states->LinearClamp());

    DX::FindMediaFile(strFilePath, MAX_PATH, L"arrow.png");
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, strFilePath, m_cursor.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_cursor.Get(),
        m_resourceDescriptors->GetCpuHandle(Descriptors::ArrowTex));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // Mouse::Resolution for GDK combo is handled in MainGDK.cpp

#if defined(_XBOX_ONE) && defined(_TITLE)
    if (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_Enable4K_UHD)
    {
        Mouse::SetDpi(192.f);
    }
#elif defined(UWP)
    if (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_Enable4K_Xbox)
    {
        Mouse::SetDpi(192.f);
    }
    else if (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableQHD_Xbox)
    {
        Mouse::SetDpi(128.f);
    }
#endif

    const auto size = m_deviceResources->GetOutputSize();
    const auto proj = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(70.f), float(size.right) / float(size.bottom), 0.01f, 100.f);

    m_roomEffect->SetProjection(proj);

    const auto viewPort = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewPort);

#ifdef COMBO_GDK
#elif defined(XBOX)
    unsigned int resflags = DX::DeviceResources::c_Enable4K_UHD;
#if _GAMING_XBOX
    resflags |= DX::DeviceResources::c_EnableQHD;
#endif
    if (m_deviceResources->GetDeviceOptions() & resflags)
    {
        // Scale sprite batch rendering when running >1080p
        static const D3D12_VIEWPORT s_vp1080 = { 0.f, 0.f, 1920.f, 1080.f, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        m_spriteBatch->SetViewport(s_vp1080);
    }
#elif defined(UWP)
    if (m_deviceResources->GetDeviceOptions() & (DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox))
    {
        // Scale sprite batch rendering when running 4k or 1440p
        static const D3D12_VIEWPORT s_vp1080 = { 0.f, 0.f, 1920.f, 1080.f, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        m_spriteBatch->SetViewport(s_vp1080);
    }

    auto rotation = m_deviceResources->GetRotation();
    m_spriteBatch->SetRotation(rotation);
#endif
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_roomEffect.reset();
    m_room.reset();
    m_spriteBatch.reset();
    m_comicFont.reset();
    m_roomTex.Reset();
    m_cursor.Reset();
    m_states.reset();
    m_resourceDescriptors.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion
