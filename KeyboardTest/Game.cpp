//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Keyboard
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

#define GAMMA_CORRECT_RENDERING

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const XMVECTORF32 START_POSITION = { 0.f, -1.5f, 0.f, 0.f };
    const XMVECTORF32 ROOM_BOUNDS = { 8.f, 6.f, 12.f, 0.f };

    const float MOVEMENT_GAIN = 0.07f;
}

Game::Game() noexcept(false) :
    m_kb{},
    m_lastStr(nullptr),
    m_lastStrBuff{}
{
    m_cameraPos = START_POSITION.v;

#ifdef GAMMA_CORRECT_RENDERING
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

#if defined(_XBOX_ONE) && defined(_TITLE)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD);
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D24_UNORM_S8_UINT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
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
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
    HWND window,
#else
    IUnknown* window,
#endif
    int width, int height, DXGI_MODE_ROTATION rotation)
{
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

    // Singleton test
    {
        bool thrown = false;

        try
        {
            std::unique_ptr<Keyboard> kb2(new Keyboard);
        }
        catch (...)
        {
            thrown = true;
        }

        if (!thrown)
        {
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
            MessageBoxW(window, L"Keyboard not acting like a singleton", L"KeyboardTest", MB_ICONERROR);
#else
            throw std::exception("Keyboard not acting like a singleton");
#endif
        }

        auto state = Keyboard::Get().GetState();
        state;
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

    Vector3 halfBound = (Vector3(ROOM_BOUNDS.v) / Vector3(2.f)) - Vector3(0.1f, 0.1f, 0.1f);

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

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    XMVECTOR lookAt = XMVectorAdd(m_cameraPos, Vector3::Backward);

    XMMATRIX view = XMMatrixLookAtRH(m_cameraPos, lookAt, Vector3::Up);

    m_roomEffect->SetView(view);
    m_roomEffect->Apply(commandList);
    m_room->Draw(commandList);

    XMVECTOR xsize = m_comicFont->MeasureString(L"X");

    float width = XMVectorGetX(xsize);
    float height = XMVectorGetY(xsize);

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
    pos.y += height * 2;

    for (int vk = 0x30; vk <= 0x39; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x30);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    // Row 2
    pos.x = 50;
    pos.y += height * 2;

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

    // Row 3
    pos.x = 50;
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"LeftShift", pos, m_kb.LeftShift ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"RightShift", pos, m_kb.RightShift ? red : lightGray);

    pos.x += width * 15;

    for (int vk = 0x67; vk <= 0x69; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x60);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    // Row 4
    pos.x = 50;
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"LeftCtrl", pos, m_kb.LeftControl ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"RightCtrl", pos, m_kb.RightControl ? red : lightGray);

    pos.x += width * 15;

    for (int vk = 0x64; vk <= 0x66; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x60);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    // Row 5
    pos.x = 50;
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"LeftAlt", pos, m_kb.LeftAlt ? red : lightGray);

    pos.x += width * 10;

    m_comicFont->DrawString(m_spriteBatch.get(), L"RightAlt", pos, m_kb.RightAlt ? red : lightGray);

    pos.x += width * 15;

    for (int vk = 0x61; vk <= 0x63; ++vk)
    {
        wchar_t buff[3] = {};
        swprintf_s(buff, L"%d", vk - 0x60);
        m_comicFont->DrawString(m_spriteBatch.get(), buff, pos, m_kb.IsKeyDown(static_cast<DirectX::Keyboard::Keys>(vk)) ? red : lightGray);

        pos.x += width * 2;
    }

    // Row 6
    pos.x = 50;
    pos.y += height * 2;

    m_comicFont->DrawString(m_spriteBatch.get(), L"Space", pos, m_kb.Space ? red : lightGray);

    pos.x += width * 25;

    m_comicFont->DrawString(m_spriteBatch.get(), "0", pos, m_kb.IsKeyDown(DirectX::Keyboard::Keys::NumPad0) ? red : lightGray);

    if (m_lastStr)
    {
        m_comicFont->DrawString(m_spriteBatch.get(), m_lastStr, XMFLOAT2(50, 650), yellow);
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

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    auto proj = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(70.f), float(size.right) / float(size.bottom), 0.01f, 100.f);

    m_roomEffect->SetProjection(proj);

    auto viewPort = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewPort);

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP) 
    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());
#endif
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
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
