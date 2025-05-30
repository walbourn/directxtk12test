//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK SpriteFont
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#define GAMMA_CORRECT_RENDERING

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float SWAP_TIME = 3.f;

    constexpr float EPSILON = 0.000001f;

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
}

Game::Game() noexcept(false) :
    m_frame(0),
    m_showUTF8(false),
    m_delay(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

    // 2D only rendering
#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_UNKNOWN, 2,
        DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_UNKNOWN, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat, DXGI_FORMAT_UNKNOWN);
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

    UnitTests();

    m_delay = SWAP_TIME;
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
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    auto kb = m_keyboard->GetState();
    if (kb.Escape || (pad.IsConnected() && pad.IsViewPressed()))
    {
        ExitGame();
    }

    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    m_keyboardButtons.Update(kb);

    if (kb.Left || (pad.IsConnected() && pad.dpad.left))
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_ROTATE270);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_ROTATE270);
    }
    else if (kb.Right || (pad.IsConnected() && pad.dpad.right))
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_ROTATE90);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_ROTATE90);
    }
    else if (kb.Up || (pad.IsConnected() && pad.dpad.up))
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_IDENTITY);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_IDENTITY);
    }
    else if (kb.Down || (pad.IsConnected() && pad.dpad.down))
    {
        m_spriteBatch->SetRotation(DXGI_MODE_ROTATION_ROTATE180);
        assert(m_spriteBatch->GetRotation() == DXGI_MODE_ROTATION_ROTATE180);
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space) || (m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED))
    {
        m_showUTF8 = !m_showUTF8;
        m_delay = SWAP_TIME;
    }
    else if (!kb.Space && !(pad.IsConnected() && pad.IsYPressed()))
    {
        m_delay -= static_cast<float>(timer.GetElapsedSeconds());

        if (m_delay <= 0.f)
        {
            m_showUTF8 = !m_showUTF8;
            m_delay = SWAP_TIME;
        }
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

    XMVECTORF32 red, blue, dgreen, cyan, yellow, gray, dgray;
#ifdef GAMMA_CORRECT_RENDERING
    red.v = XMColorSRGBToRGB(Colors::Red);
    blue.v = XMColorSRGBToRGB(Colors::Blue);
    dgreen.v = XMColorSRGBToRGB(Colors::DarkGreen);
    cyan.v = XMColorSRGBToRGB(Colors::Cyan);
    yellow.v = XMColorSRGBToRGB(Colors::Yellow);
    gray.v = XMColorSRGBToRGB(Colors::Gray);
    dgray.v = XMColorSRGBToRGB(Colors::DarkGray);
#else
    red.v = Colors::Red;
    blue.v = Colors::Blue;
    dgreen.v = Colors::DarkGreen;
    cyan.v = Colors::Cyan;
    yellow.v = Colors::Yellow;
    gray.v = Colors::Gray;
    dgray.v = Colors::DarkGray;
#endif

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

    if (m_showUTF8)
    {
        m_comicFont->DrawString(m_spriteBatch.get(), "Hello, world!", XMFLOAT2(0, 0));
        m_italicFont->DrawString(m_spriteBatch.get(), "This text is in italics.\nIs it well spaced?", XMFLOAT2(220, 0));
        m_scriptFont->DrawString(m_spriteBatch.get(), "Script font, yo...", XMFLOAT2(0, 50));

        SpriteEffects flip = (SpriteEffects)((int)(time / 100) & 3);
        m_multicoloredFont->DrawString(m_spriteBatch.get(), "OMG it's full of stars!", XMFLOAT2(610, 130), Colors::White, XM_PIDIV2, XMFLOAT2(0, 0), 1, flip);

        m_comicFont->DrawString(m_spriteBatch.get(), "This is a larger block\nof text using a\nfont scaled to a\nsmaller size.\nSome c\xffha\xferac\xfdte\xffr\xfas are not in the font, but should show up as hyphens.", XMFLOAT2(10, 90), Colors::Black, 0, XMFLOAT2(0, 0), 0.5f);

        char tmp[256] = {};
        sprintf_s(tmp, "%llu frames", m_frame);

        m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(201, 130), Colors::Black);
        m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(200, 131), Colors::Black);
        m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(200, 130), red);

        float scale = sin(time / 100) + 1;
        auto spinText = "Spinning\nlike a cat";
        Vector2 size = m_comicFont->MeasureString(spinText);
        m_comicFont->DrawString(m_spriteBatch.get(), spinText, Vector2(150, 350), blue, time / 60, size / 2, scale);

        auto mirrorText = "It's a\nmirror...";
        Vector2 mirrorSize = m_comicFont->MeasureString(mirrorText);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), Colors::Black, 0, mirrorSize * Vector2(0, 1), 1, SpriteEffects_None);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), gray, 0, mirrorSize * Vector2(1, 1), 1, SpriteEffects_FlipHorizontally);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), gray, 0, mirrorSize * Vector2(0, 0), 1, SpriteEffects_FlipVertically);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), dgray, 0, mirrorSize * Vector2(1, 0), 1, SpriteEffects_FlipBoth);

        m_japaneseFont->DrawString(m_spriteBatch.get(), L"\x79C1\x306F\x65E5\x672C\x8A9E\x304C\x8A71\x305B\x306A\x3044\x306E\x3067\x3001\n\x79C1\x306F\x3053\x308C\x304C\x4F55\x3092\x610F\x5473\x3059\x308B\x306E\x304B\x308F\x304B\x308A\x307E\x305B\x3093", XMFLOAT2(10, 512));
    }
    else
    {
        m_comicFont->DrawString(m_spriteBatch.get(), L"Hello, world!", XMFLOAT2(0, 0));
        m_italicFont->DrawString(m_spriteBatch.get(), L"This text is in italics.\nIs it well spaced?", XMFLOAT2(220, 0));
        m_scriptFont->DrawString(m_spriteBatch.get(), L"Script font, yo...", XMFLOAT2(0, 50));

        SpriteEffects flip = (SpriteEffects)((int)(time / 100) & 3);
        m_multicoloredFont->DrawString(m_spriteBatch.get(), L"OMG it's full of stars!", XMFLOAT2(610, 130), Colors::White, XM_PIDIV2, XMFLOAT2(0, 0), 1, flip);

        m_comicFont->DrawString(m_spriteBatch.get(), L"This is a larger block\nof text using a\nfont scaled to a\nsmaller size.\nSome c\x1234ha\x1543rac\x2453te\x1634r\x1563s are not in the font, but should show up as hyphens.", XMFLOAT2(10, 90), Colors::Black, 0, XMFLOAT2(0, 0), 0.5f);

        wchar_t tmp[256] = {};
        swprintf_s(tmp, L"%llu frames", m_frame);

        m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(201, 130), Colors::Black);
        m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(200, 131), Colors::Black);
        m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(200, 130), red);

        float scale = sin(time / 100) + 1;
        auto spinText = L"Spinning\nlike a cat";
        Vector2 size = m_comicFont->MeasureString(spinText);
        m_comicFont->DrawString(m_spriteBatch.get(), spinText, Vector2(150, 350), blue, time / 60, size / 2, scale);

        auto mirrorText = L"It's a\nmirror...";
        Vector2 mirrorSize = m_comicFont->MeasureString(mirrorText);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), Colors::Black, 0, mirrorSize * Vector2(0, 1), 1, SpriteEffects_None);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), gray, 0, mirrorSize * Vector2(1, 1), 1, SpriteEffects_FlipHorizontally);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), gray, 0, mirrorSize * Vector2(0, 0), 1, SpriteEffects_FlipVertically);
        m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, Vector2(400, 400), dgray, 0, mirrorSize * Vector2(1, 0), 1, SpriteEffects_FlipBoth);

        m_japaneseFont->DrawString(m_spriteBatch.get(), L"\x79C1\x306F\x65E5\x672C\x8A9E\x304C\x8A71\x305B\x306A\x3044\x306E\x3067\x3001\n\x79C1\x306F\x3053\x308C\x304C\x4F55\x3092\x610F\x5473\x3059\x308B\x306E\x304B\x308F\x304B\x308A\x307E\x305B\x3093", XMFLOAT2(10, 512));
    }

    {
        char ascii[256] = {};
        int i = 0;
        for (size_t j = 32; j < 256; ++j)
        {
            if (j == L'\n' || j == L'\r' || j == L'\t')
                continue;

            if (j > 0 && (j % 128) == 0)
            {
                ascii[i++] = L'\n';
                ascii[i++] = L'\n';
            }

            ascii[i++] = static_cast<char>(j + 1);
        }

        UINT cp = 437;
    #ifdef XBOX
        cp = CP_UTF8;

        m_consolasFont->SetDefaultCharacter('-');
    #endif

        wchar_t unicode[256] = {};
        if (!MultiByteToWideChar(cp, 0, ascii, i, unicode, 256))
            swprintf_s(unicode, L"<ERROR: %u>\n", GetLastError());

        m_consolasFont->DrawString(m_spriteBatch.get(), unicode, XMFLOAT2(10, 600), cyan);
    }

    m_spriteBatch->End();

    m_spriteBatch->Begin(commandList);

    m_ctrlFont->DrawString(m_spriteBatch.get(), L" !\"\n#$%\n&'()\n*+,-", XMFLOAT2(650, 130), Colors::White, 0.f, XMFLOAT2(0.f, 0.f), 0.5f);

    m_ctrlOneFont->DrawString(m_spriteBatch.get(), L" !\"\n#$%\n&'()\n*+,-", XMFLOAT2(950, 130), Colors::White, 0.f, XMFLOAT2(0.f, 0.f), 0.5f);

#ifndef XBOX
    {
        LONG w, h;

        auto outputSize = m_deviceResources->GetOutputSize();

        switch (m_spriteBatch->GetRotation())
        {
        case DXGI_MODE_ROTATION_ROTATE90:
        case DXGI_MODE_ROTATION_ROTATE270:
            w = outputSize.bottom;
            h = outputSize.right;
            break;

        default:
            w = outputSize.right;
            h = outputSize.bottom;
            break;
        }

        for (LONG x = 0; x < w; x += 100)
        {
            wchar_t tmp[16] = {};
            swprintf_s(tmp, L"%d\n", x);
            m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(float(x), float(h - 50)), yellow);
        }

        for (LONG y = 0; y < h; y += 100)
        {
            wchar_t tmp[16] = {};
            swprintf_s(tmp, L"%d\n", y);
            m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(float(w - 100), float(y)), red);
        }
    }
#endif

    m_spriteBatch->End();

    m_spriteBatch->Begin(commandList);

    const RECT r = { 640, 20, 740, 38 };
    commandList->RSSetScissorRects(1, &r);

    m_comicFont->DrawString(m_spriteBatch.get(), L"Clipping!", XMFLOAT2(640, 0), dgreen);

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

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
    commandList->ClearRenderTargetView(rtvDescriptor, c_clearColor, 0, nullptr);

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
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Game::OnSuspending()
{
    m_deviceResources->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
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

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

    {
        const SpriteBatchPipelineStateDescription pd(rtState);

        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

#ifdef GAMMA_CORRECT_RENDERING
    bool forceSRGB = true;
#else
    bool forceSRGB = false;
#endif

    m_comicFont = std::make_unique<SpriteFont>(device, resourceUpload, L"comic.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ComicFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ComicFont));

    m_italicFont = std::make_unique<SpriteFont>(device, resourceUpload, L"italic.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ItalicFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ItalicFont));

    m_scriptFont = std::make_unique<SpriteFont>(device, resourceUpload, L"script.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ScriptFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ScriptFont));

    m_nonproportionalFont = std::make_unique<SpriteFont>(device, resourceUpload, L"nonproportional.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::NonProportionalFont), m_resourceDescriptors->GetGpuHandle(Descriptors::NonProportionalFont));

    m_multicoloredFont = std::make_unique<SpriteFont>(device, resourceUpload, L"multicolored.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::MulticoloredFont), m_resourceDescriptors->GetGpuHandle(Descriptors::MulticoloredFont), forceSRGB);

    m_japaneseFont = std::make_unique<SpriteFont>(device, resourceUpload, L"japanese.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::JapaneseFont), m_resourceDescriptors->GetGpuHandle(Descriptors::JapaneseFont));

    m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload, L"xboxController.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::CtrlFont), m_resourceDescriptors->GetGpuHandle(Descriptors::CtrlFont), forceSRGB);

    m_ctrlOneFont = std::make_unique<SpriteFont>(device, resourceUpload, L"xboxOneController.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::CtrlOneFont), m_resourceDescriptors->GetGpuHandle(Descriptors::CtrlOneFont), forceSRGB);

    m_consolasFont = std::make_unique<SpriteFont>(device, resourceUpload, L"consolas.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::ConsolasFont), m_resourceDescriptors->GetGpuHandle(Descriptors::ConsolasFont));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    const auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);

#ifdef XBOX
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
    m_comicFont.reset();
    m_italicFont.reset();
    m_scriptFont.reset();
    m_nonproportionalFont.reset();
    m_multicoloredFont.reset();
    m_japaneseFont.reset();
    m_ctrlFont.reset();
    m_ctrlOneFont.reset();
    m_consolasFont.reset();

    m_resourceDescriptors.reset();
    m_spriteBatch.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();

    m_comicFont->SetDefaultCharacter('-');
}
#endif
#pragma endregion

void Game::UnitTests()
{
    OutputDebugStringA("*********** UNIT TESTS BEGIN ***************\n");

    bool success = true;

#ifndef NO_WCHAR_T
    // GetDefaultCharacterTest
    if (m_comicFont->GetDefaultCharacter() != 0)
    {
        OutputDebugStringA("FAILED: GetDefaultCharacter\n");
        success = false;
    }
#endif

    // ContainsCharacter tests
    if (m_comicFont->ContainsCharacter(27)
        || !m_comicFont->ContainsCharacter('-'))
    {
        OutputDebugStringA("FAILED: ContainsCharacter\n");
        success = false;
    }

    // FindGlyph/GetSpriteSheet tests
    {
        auto g = m_comicFont->FindGlyph('-');
        if (g->Character != '-' || g->XOffset != 6 || g->YOffset != 24)
        {
            OutputDebugStringA("FAILED: FindGlyph\n");
            success = false;
        }

        const auto spriteSheet = m_comicFont->GetSpriteSheet();

        if (!spriteSheet.ptr)
        {
            OutputDebugStringA("FAILED: GetSpriteSheet\n");
            success = false;
        }

        auto spriteSheetSize = m_comicFont->GetSpriteSheetSize();
        if (spriteSheetSize.x != 256
            || spriteSheetSize.y != 172)
        {
            OutputDebugStringA("FAILED: GetSpriteSheetSize\n");
            success = false;
        }
    }

    // DefaultCharacter tests
    m_comicFont->SetDefaultCharacter('-');
#ifndef NO_WCHAR_T
    if (m_comicFont->GetDefaultCharacter() != '-')
    {
        OutputDebugStringA("FAILED: Get/SetDefaultCharacter\n");
        success = false;
    }
#endif

    // Linespacing tests
    float s = m_ctrlFont->GetLineSpacing();
    if (s != 186.f)
    {
        OutputDebugStringA("FAILED: GetLineSpacing\n");
        success = false;
    }
    m_ctrlFont->SetLineSpacing(256.f);
    s = m_ctrlFont->GetLineSpacing();
    if (s != 256.f)
    {
        OutputDebugStringA("FAILED: Get/SetLineSpacing failed\n");
        success = false;
    }
    m_ctrlFont->SetLineSpacing(186.f);

    // Measure tests
    {
        const auto spinText = L"Spinning\nlike a cat";

        auto drawSize = m_comicFont->MeasureString(spinText);

        if (fabs(XMVectorGetX(drawSize) - 136.f) > EPSILON
            || fabs(XMVectorGetY(drawSize) - 85.4713516f) > EPSILON)
        {
            OutputDebugStringA("FAILED: MeasureString\n");
            success = false;
        }

        auto rect = m_comicFont->MeasureDrawBounds(spinText, XMFLOAT2(150, 350));

        if (rect.top != 361
            || rect.bottom != 428
            || rect.left != 157
            || rect.right != 286)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds\n");
            success = false;
        }

        auto text2 = L"  Spinning like a cat ";

        drawSize = m_comicFont->MeasureString(text2);

        if (fabs(XMVectorGetX(drawSize) - 284.f) > EPSILON
            || fabs(XMVectorGetY(drawSize) - 44.f) > EPSILON)
        {
            OutputDebugStringA("FAILED: MeasureString (2)\n");
            success = false;
        }

        rect = m_comicFont->MeasureDrawBounds(text2, XMFLOAT2(150, 350));

        if (rect.top != 359
            || rect.bottom != 394
            || rect.left != 175
            || rect.right != 434)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds (2)\n");
            success = false;
        }

        // test measurements that factor in whitespace characters
        // against measurements that do not factor in whitespace characters

        auto text3 = L"aeiou";
        auto text4 = L"a e i o u";
        auto text5 = L"aeiou ";
        auto text6 = L"  ";
        auto testLineHeight = m_comicFont->GetLineSpacing();

        const auto testPos = XMFLOAT2{ 10.0f, 15.0f };

        // this test makes sure that including whitespace in the measurements
        // for a string that does not include whitespace results in no change

        auto drawSize3_1 = m_comicFont->MeasureString(text3, true);
        auto drawSize3_2 = m_comicFont->MeasureString(text3, false);

        if (XMVectorGetX(drawSize3_1) != XMVectorGetX(drawSize3_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #1\n");
            success = false;
        }

        if (XMVectorGetY(drawSize3_2) != testLineHeight || XMVectorGetY(drawSize3_1) != XMVectorGetY(drawSize3_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #2\n");
            success = false;
        }

        // this test makes sure that including whitespace in the measurements
        // for a string that does include whitespace still results in no change
        // if the whitespace is in the middle

        auto drawSize4_1 = m_comicFont->MeasureString(text4, true);
        auto drawSize4_2 = m_comicFont->MeasureString(text4, false);

        if (XMVectorGetX(drawSize4_1) != XMVectorGetX(drawSize4_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #3\n");
            success = false;
        }

        if (XMVectorGetY(drawSize4_2) != testLineHeight || XMVectorGetY(drawSize4_1) != XMVectorGetY(drawSize4_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #4\n");
            success = false;
        }

        // this test makes sure that including whitespace in the measurements
        // for a string that does include whitespace results in change
        // if the whitespace is at the end for x, no change for y

        auto drawSize5_1 = m_comicFont->MeasureString(text5, true);
        auto drawSize5_2 = m_comicFont->MeasureString(text5, false);

        if (XMVectorGetX(drawSize5_1) >= XMVectorGetX(drawSize5_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #5\n");
            success = false;
        }

        if (XMVectorGetY(drawSize5_2) != testLineHeight || XMVectorGetY(drawSize5_1) != XMVectorGetY(drawSize5_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #6\n");
            success = false;
        }

        // this test makes sure that including whitespace in the measurements
        // for a string that only includes whitespace results in change

        auto drawSize6_1 = m_comicFont->MeasureString(text6, true);
        auto drawSize6_2 = m_comicFont->MeasureString(text6, false);

        if (XMVectorGetX(drawSize6_1) >= XMVectorGetX(drawSize6_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #7\n");
            success = false;
        }

        if (XMVectorGetY(drawSize6_2) != testLineHeight || XMVectorGetY(drawSize6_1) >= XMVectorGetY(drawSize6_2))
        {
            OutputDebugStringA("FAILED: MeasureString ignoreSpace vs. !ignoreSpace test #8\n");
            success = false;
        }

        // this test makes sure that including whitespace in the measurements
        // for a draw bounds that does not include whitespace results in no change

        auto rect3_1 = m_comicFont->MeasureDrawBounds(text3, testPos, true);
        auto rect3_2 = m_comicFont->MeasureDrawBounds(text3, testPos, false);

        if (rect3_1.left != rect3_2.left || rect3_1.top != rect3_2.top ||
            rect3_1.right != rect3_2.right || rect3_1.bottom != rect3_2.bottom)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds ignoreSpace vs. !ignoreSpace test #1\n");
            success = false;
        }

        // this test makes sure that including whitespace in the measurements
        // for a string that does include whitespace results still in no change
        // if the whitespace is in the middle for x, but results in change for y

        auto rect4_1 = m_comicFont->MeasureDrawBounds(text4, testPos, true);
        auto rect4_2 = m_comicFont->MeasureDrawBounds(text4, testPos, false);

        if (rect4_1.left != rect4_2.left || rect4_1.right != rect4_2.right)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds ignoreSpace vs. !ignoreSpace test #2\n");
            success = false;
        }

        if (rect4_1.top <= rect4_2.top || rect4_1.bottom >= rect4_2.bottom)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds ignoreSpace vs. !ignoreSpace test #3\n");
            success = false;
        }

        // this test makes sure that including whitespace in the measurements
        // for a string that does include whitespace results in change
        // if the whitespace is at the end for x, and also change for y

        auto rect5_1 = m_comicFont->MeasureDrawBounds(text5, testPos, true);
        auto rect5_2 = m_comicFont->MeasureDrawBounds(text5, testPos, false);

        if (rect5_1.left != rect5_2.left || rect5_1.right >= rect5_2.right)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds ignoreSpace vs. !ignoreSpace test #4\n");
            success = false;
        }

        if (rect5_1.top <= rect5_2.top || rect5_1.bottom >= rect5_2.bottom)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds ignoreSpace vs. !ignoreSpace test #5\n");
            success = false;
        }

        // this test makes sure that including whitespace in the measurements
        // for a draw bounds that only includes whitespace results in change

        auto rect6_1 = m_comicFont->MeasureDrawBounds(text6, testPos, true);
        auto rect6_2 = m_comicFont->MeasureDrawBounds(text6, testPos, false);

        if (rect6_1.left >= rect6_2.left || rect6_1.right >= rect6_2.right)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds ignoreSpace vs. !ignoreSpace test #6\n");
            success = false;
        }

        if (rect6_1.top >= rect6_2.top || rect6_1.bottom >= rect6_2.bottom)
        {
            OutputDebugStringA("FAILED: MeasureDrawBounds ignoreSpace vs. !ignoreSpace test #7\n");
            success = false;
        }
    }

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");

    if (!success)
    {
        throw std::runtime_error("Unit Tests Failed");
    }
}
