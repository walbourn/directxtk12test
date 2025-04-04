//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Keyboard
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

    constexpr float MOVEMENT_GAIN = 0.07f;

    constexpr int c_oemLowOffset = 0xBA;
    constexpr const wchar_t* s_oemLow[] =
    {
        L"Semicolon",   // Oem 1
        L"Plus",
        L"Comma",
        L"Minus",
        L"Period",
        L"Question",    // Oem 2
        L"Tilde",       // Oem 3
    };

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
}

static_assert(std::is_nothrow_move_constructible<Keyboard>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<Keyboard>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<Keyboard::KeyboardStateTracker>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<Keyboard::KeyboardStateTracker>::value, "Move Assign.");

Game::Game() noexcept(false) :
    m_kb{},
    m_frame(0),
    m_lastStr(nullptr),
    m_lastStrBuff{}
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

#ifdef COMBO_GDK
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
#elif defined(XBOX)
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
            auto kb2 = std::make_unique<Keyboard>();
        }
        catch (...)
        {
            thrown = true;
        }

        if (!thrown)
        {
#ifdef PC
            MessageBoxW(window, L"Keyboard not acting like a singleton", L"KeyboardTest", MB_ICONERROR);
#else
            throw std::runtime_error("Keyboard not acting like a singleton");
#endif
        }
    }

    OutputDebugStringA(m_keyboard->IsConnected() ? "INFO: Keyboard is connected\n" : "INFO: No keyboard found\n");

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
    ++m_frame;
}

// Updates the world.
void Game::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto kb = m_keyboard->GetState();

    if (kb.Escape)
    {
        ExitGame();
    }

    if (kb.Home)
        m_keyboard->Reset();

    m_tracker.Update(kb);

    if (m_tracker.pressed.Q)
        m_lastStr = L"Q was pressed";
    else if (m_tracker.released.Q)
        m_lastStr = L"Q was released";
    else if (m_tracker.pressed.W)
        m_lastStr = L"W was pressed";
    else if (m_tracker.released.W)
        m_lastStr = L"W was released";
    else if (m_tracker.pressed.E)
        m_lastStr = L"E was pressed";
    else if (m_tracker.released.E)
        m_lastStr = L"E was released";
    else if (m_tracker.pressed.R)
        m_lastStr = L"R was pressed";
    else if (m_tracker.released.R)
        m_lastStr = L"R was released";
    else if (m_tracker.pressed.T)
        m_lastStr = L"T was pressed";
    else if (m_tracker.released.T)
        m_lastStr = L"T was released";
    else if (m_tracker.pressed.Y)
        m_lastStr = L"Y was pressed";
    else if (m_tracker.released.Y)
        m_lastStr = L"Y was released";
    else if (m_tracker.pressed.LeftShift)
        m_lastStr = L"LeftShift was pressed";
    else if (m_tracker.released.LeftShift)
        m_lastStr = L"LeftShift was released";
    else if (m_tracker.pressed.LeftAlt)
        m_lastStr = L"LeftAlt was pressed";
    else if (m_tracker.released.LeftAlt)
        m_lastStr = L"LeftAlt was released";
    else if (m_tracker.pressed.LeftControl)
        m_lastStr = L"LeftCtrl was pressed";
    else if (m_tracker.released.LeftControl)
        m_lastStr = L"LeftCtrl was released";
    else if (m_tracker.pressed.RightShift)
        m_lastStr = L"RightShift was pressed";
    else if (m_tracker.released.RightShift)
        m_lastStr = L"RightShift was released";
    else if (m_tracker.pressed.RightAlt)
        m_lastStr = L"RightAlt was pressed";
    else if (m_tracker.released.RightAlt)
        m_lastStr = L"RightAlt was released";
    else if (m_tracker.pressed.RightControl)
        m_lastStr = L"RightCtrl was pressed";
    else if (m_tracker.released.RightControl)
        m_lastStr = L"RightCtrl was released";
    else if (m_tracker.pressed.Space)
        m_lastStr = L"Space was pressed";
    else if (m_tracker.released.Space)
        m_lastStr = L"Space was released";
    else if (m_tracker.pressed.Enter)
        m_lastStr = L"Enter was pressed";
    else if (m_tracker.released.Enter)
        m_lastStr = L"Enter was released";
    else if (m_tracker.pressed.Back)
        m_lastStr = L"Back was pressed";
    else if (m_tracker.released.Back)
        m_lastStr = L"Back was released";
    else if (m_tracker.pressed.NumLock)
        m_lastStr = L"NumLock was pressed";
    else if (m_tracker.released.NumLock)
        m_lastStr = L"NumLock was released";
    else if (m_tracker.pressed.CapsLock)
        m_lastStr = L"CapsLock was pressed";
    else if (m_tracker.released.CapsLock)
        m_lastStr = L"CapsLock was released";
    else if (m_tracker.pressed.Scroll)
        m_lastStr = L"Scroll was pressed";
    else if (m_tracker.released.Scroll)
        m_lastStr = L"Scroll was released";
    else if (m_tracker.pressed.Up)
        m_lastStr = L"Up was pressed";
    else if (m_tracker.released.Up)
        m_lastStr = L"Up was released";
    else if (m_tracker.pressed.Down)
        m_lastStr = L"Down was presssed";
    else if (m_tracker.released.Down)
        m_lastStr = L"Down was released";
    else if (m_tracker.pressed.Left)
        m_lastStr = L"Left was pressed";
    else if (m_tracker.released.Left)
        m_lastStr = L"Left was released";
    else if (m_tracker.pressed.Right)
        m_lastStr = L"Right was pressed";
    else if (m_tracker.released.Right)
        m_lastStr = L"Right was released";
    else if (m_tracker.pressed.PageUp)
        m_lastStr = L"PageUp was pressed";
    else if (m_tracker.released.PageUp)
        m_lastStr = L"PageUp was released";
    else if (m_tracker.pressed.PageDown)
        m_lastStr = L"PageDown was pressed";
    else if (m_tracker.released.PageDown)
        m_lastStr = L"PageDown was released";

    for (int vk = VK_F1; vk <= VK_F10; ++vk)
    {
        if (m_tracker.IsKeyPressed(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"F%d was pressed", vk - VK_F1 + 1);
            m_lastStr = m_lastStrBuff;
        }
        else if (m_tracker.IsKeyReleased(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"F%d was released", vk - VK_F1 + 1);
            m_lastStr = m_lastStrBuff;
        }
    }

    for (int vk = 0x30; vk <= 0x39; ++vk)
    {
        if (m_tracker.IsKeyPressed(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"%d was pressed", vk - 0x30);
            m_lastStr = m_lastStrBuff;
        }
        else if (m_tracker.IsKeyReleased(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"%d was released", vk - 0x30);
            m_lastStr = m_lastStrBuff;
        }
    }

    for (int vk = 0x60; vk <= 0x69; ++vk)
    {
        if (m_tracker.IsKeyPressed(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"Numkey %d was pressed", vk - 0x60);
            m_lastStr = m_lastStrBuff;
        }
        else if (m_tracker.IsKeyReleased(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"Numkey %d was released", vk - 0x60);
            m_lastStr = m_lastStrBuff;
        }
    }

    for (int vk = 0xBA; vk <= 0xC0; ++vk)
    {
        if (m_tracker.IsKeyPressed(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"Oem%ls was pressed", s_oemLow[vk - c_oemLowOffset]);
            m_lastStr = m_lastStrBuff;
        }
        else if (m_tracker.IsKeyReleased(static_cast<DirectX::Keyboard::Keys>(vk)))
        {
            swprintf_s(m_lastStrBuff, L"Oem%ls was released", s_oemLow[vk - c_oemLowOffset]);
            m_lastStr = m_lastStrBuff;
        }
    }

    Vector3 move = Vector3::Zero;

    if (kb.Up)
        move.y += 1.f;

    if (kb.Down)
        move.y -= 1.f;

    if (kb.Left)
        move.x -= 1.f;

    if (kb.Right)
        move.x += 1.f;

    if (kb.PageUp)
        move.z += 1.f;

    if (kb.PageDown)
        move.z -= 1.f;

    move *= MOVEMENT_GAIN;

    m_cameraPos += move;

    const Vector3 halfBound = (Vector3(ROOM_BOUNDS.v) / Vector3(2.f)) - Vector3(0.1f, 0.1f, 0.1f);

    m_cameraPos = Vector3::Min(m_cameraPos, halfBound);
    m_cameraPos = Vector3::Max(m_cameraPos, -halfBound);

    m_kb = kb;

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

    XMVECTORF32 red, lightGray, yellow;
#ifdef GAMMA_CORRECT_RENDERING
    red.v = XMColorSRGBToRGB(Colors::Red);
    lightGray.v = XMColorSRGBToRGB(Colors::LightGray);
    yellow.v = XMColorSRGBToRGB(Colors::Yellow);
#else
    red.v = Colors::Red;
    lightGray.v = Colors::LightGray;
    yellow.v = Colors::Yellow;
#endif

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    ID3D12DescriptorHeap* const heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    const XMVECTOR lookAt = XMVectorAdd(m_cameraPos, Vector3::Backward);

    const XMMATRIX view = XMMatrixLookAtRH(m_cameraPos, lookAt, Vector3::Up);

    m_roomEffect->SetView(view);
    m_roomEffect->Apply(commandList);
    m_room->Draw(commandList);

    const XMVECTOR xsize = m_comicFont->MeasureString(L"X");

    const float width = XMVectorGetX(xsize);
    const float height = XMVectorGetY(xsize);
    const float linespace = height * 1.5f;

    m_spriteBatch->Begin(commandList);

    XMFLOAT2 pos(50, 50);

    // Row 0
    for (int vk = VK_F1; vk <= VK_F10; ++vk)
    {
        wchar_t buff[5] = {};
        swprintf_s(buff, L"F%d", vk - VK_F1 + 1);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 3;
    }

    // Row 1
    pos.x = 50;
    pos.y += linespace;

    for (int vk = 0x30; vk <= 0x39; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x30);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    pos.x += width * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"Q", pos, m_kb.Q ? red : lightGray);

    pos.x += width * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"W", pos, m_kb.W ? red : lightGray);

    pos.x += width * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"E", pos, m_kb.E ? red : lightGray);

    pos.x += width * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"R", pos, m_kb.R ? red : lightGray);

    pos.x += width * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"T", pos, m_kb.T ? red : lightGray);

    pos.x += width * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"Y", pos, m_kb.Y ? red : lightGray);

    // Row 2
    pos.x = 50;
    pos.y += linespace;

    m_comicFont->DrawString(m_spriteBatch.get(), L"LeftShift", pos, m_kb.LeftShift ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"RightShift", pos, m_kb.RightShift ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"NumLock", pos, m_kb.NumLock ? red : lightGray);

    pos.x += width * 10;

    for (int vk = 0x67; vk <= 0x69; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x60);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    // Row 3
    pos.x = 50;
    pos.y += linespace;

    m_comicFont->DrawString(m_spriteBatch.get(), L"LeftCtrl", pos, m_kb.LeftControl ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"RightCtrl", pos, m_kb.RightControl ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"CapsLock", pos, m_kb.CapsLock ? red : lightGray);

    pos.x += width * 10;

    for (int vk = 0x64; vk <= 0x66; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x60);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    // Row 4
    pos.x = 50;
    pos.y += linespace;

    m_comicFont->DrawString(m_spriteBatch.get(), L"LeftAlt", pos, m_kb.LeftAlt ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"RightAlt", pos, m_kb.RightAlt ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"Scroll", pos, m_kb.Scroll ? red : lightGray);

    pos.x += width * 10;

    for (int vk = 0x61; vk <= 0x63; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x60);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    // Row 5
    pos.x = 50;
    pos.y += linespace;

    m_comicFont->DrawString(m_spriteBatch.get(), L"Space", pos, m_kb.Space ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"Enter", pos, m_kb.Enter ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"Back", pos, m_kb.Back ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), "0", pos, m_kb.IsKeyDown(DirectX::Keyboard::Keys::NumPad0) ? red : lightGray);

    // Row 6
    pos.x = 50;
    pos.y += linespace;

    for (int vk = 0xBA; vk <= 0xC0; ++vk)
    {
        wchar_t buff[16] = {};
        swprintf_s(buff, L"Oem%ls", s_oemLow[vk - c_oemLowOffset]);

        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        if (vk == 0xbd)
        {
            pos.x = 50;
            pos.y += height;
        }
        else
        {
            pos.x += width * 10;
        }
    }

    // Row 7
    pos.x = 50;
    pos.y += height * 2;

    if (m_lastStr)
    {
        m_comicFont->DrawString(m_spriteBatch.get(), m_lastStr, pos, yellow);
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
}

void Game::OnSuspending()
{
    m_deviceResources->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_tracker.Reset();
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

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
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
