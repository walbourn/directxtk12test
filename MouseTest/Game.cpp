//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Mouse
//
// Copyright (c) Microsoft Corporation. All rights reserved.
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

namespace
{
    constexpr XMVECTORF32 START_POSITION = { { { 0.f, -1.5f, 0.f, 0.f } } };
    constexpr XMVECTORF32 ROOM_BOUNDS = { { { 8.f, 6.f, 12.f, 0.f } } };
}

Game::Game() noexcept(false) :
    m_ms{},
    m_lastMode(Mouse::MODE_ABSOLUTE),
    m_pitch(0),
    m_yaw(0),
    m_lastStr(nullptr)
{
    m_cameraPos = START_POSITION.v;

#ifdef GAMMA_CORRECT_RENDERING
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD);
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D24_UNORM_S8_UINT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
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

#ifdef XBOX
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
#ifdef COREWINDOW
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#if _XDK_VER >= 0x42D907D1
    m_mouse->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#endif
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

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitGame();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        bool isvisible = m_mouse->IsVisible();
        m_mouse->SetVisible(!isvisible);

        OutputDebugStringA(m_mouse->IsVisible() ? "INFO: Mouse cursor is visible\n" : "INFO: Mouse cursor NOT visible\n");
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Home))
    {
        m_mouse->ResetScrollWheelValue();
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::End))
    {
        if (m_lastMode == Mouse::MODE_ABSOLUTE)
        {
            m_mouse->SetMode(Mouse::MODE_RELATIVE);
        }
        else
        {
            m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
        }
    }

    auto mouse = m_mouse->GetState();
    m_lastMode = mouse.positionMode;

    if (mouse.positionMode == Mouse::MODE_RELATIVE)
    {
        const SimpleMath::Vector4 ROTATION_GAIN(0.004f, 0.004f, 0.f, 0.f);
        XMVECTOR delta = SimpleMath::Vector4(float(mouse.x), float(mouse.y), 0.f, 0.f) * ROTATION_GAIN;

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

    if (m_tracker.leftButton == ButtonState::PRESSED)
    {
        m_mouse->SetMode(Mouse::MODE_RELATIVE);
    }
    else if (m_tracker.leftButton == ButtonState::RELEASED)
    {
        m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
    }

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

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
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

    float y = sinf(m_pitch);        // vertical
    float r = cosf(m_pitch);        // in the plane
    float z = r * cosf(m_yaw);        // fwd-back
    float x = r * sinf(m_yaw);        // left-right

    XMVECTOR lookAt = XMVectorAdd(m_cameraPos, XMVectorSet(x, y, z, 0.f));

    XMMATRIX view = XMMatrixLookAtRH(m_cameraPos, lookAt, Vector3::Up);

    m_roomEffect->SetView(view);
    m_roomEffect->Apply(commandList);
    m_room->Draw(commandList);

    XMVECTOR xsize = m_comicFont->MeasureString(L"X");

    float height = XMVectorGetY(xsize);

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
    (void)m_graphicsMemory->GetStatistics();

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
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
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

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
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
        SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    m_comicFont = std::make_unique<SpriteFont>(device, resourceUpload,
        L"comic.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ComicFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::ComicFont));

#ifdef GAMMA_CORRECT_RENDERING
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_FORCE_SRGB;
#else
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_DEFAULT;
#endif

    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"texture.dds", 0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
        m_roomTex.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_roomTex.Get(),
        m_resourceDescriptors->GetCpuHandle(Descriptors::RoomTex));

    m_roomEffect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::RoomTex), m_states->LinearClamp());

    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"arrow.png", m_cursor.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_cursor.Get(),
        m_resourceDescriptors->GetCpuHandle(Descriptors::ArrowTex));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
#ifdef XBOX
    if (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_Enable4K_UHD)
    {
#ifdef GAMEINPUT
        Mouse::SetResolution(true);
#else
        Mouse::SetDpi(192.);
#endif
    }
#elif defined(UWP)
    if (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_Enable4K_Xbox)
    {
        Mouse::SetDpi(192.);
    }
#endif

    auto size = m_deviceResources->GetOutputSize();
    auto proj = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(70.f), float(size.right) / float(size.bottom), 0.01f, 100.f);

    m_roomEffect->SetProjection(proj);

    auto viewPort = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewPort);

#ifdef UWP
    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());
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
