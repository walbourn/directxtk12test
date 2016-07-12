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

Game::Game() :
    m_frame(0)
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

    ++m_frame;
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

    float time = 60.f * static_cast<float>(m_timer.GetTotalSeconds());

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
    m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(200, 130), Colors::Red);

    float scale = sin(time / 100) + 1;
    auto spinText = L"Spinning\nlike a cat";
    auto size = m_comicFont->MeasureString(spinText);
    m_comicFont->DrawString(m_spriteBatch.get(), spinText, XMVectorSet(150, 350, 0, 0), Colors::Blue, time / 60, size / 2, scale);

    auto mirrorText = L"It's a\nmirror...";
    auto mirrorSize = m_comicFont->MeasureString(mirrorText);
    m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, XMVectorSet(400, 400, 0, 0), Colors::Black, 0, mirrorSize * XMVectorSet(0, 1, 0, 0), 1, SpriteEffects_None);
    m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, XMVectorSet(400, 400, 0, 0), Colors::Gray, 0, mirrorSize * XMVectorSet(1, 1, 0, 0), 1, SpriteEffects_FlipHorizontally);
    m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, XMVectorSet(400, 400, 0, 0), Colors::Gray, 0, mirrorSize * XMVectorSet(0, 0, 0, 0), 1, SpriteEffects_FlipVertically);
    m_comicFont->DrawString(m_spriteBatch.get(), mirrorText, XMVectorSet(400, 400, 0, 0), Colors::DarkGray, 0, mirrorSize * XMVectorSet(1, 0, 0, 0), 1, SpriteEffects_FlipBoth);

    m_japaneseFont->DrawString(m_spriteBatch.get(), L"\x79C1\x306F\x65E5\x672C\x8A9E\x304C\x8A71\x305B\x306A\x3044\x306E\x3067\x3001\n\x79C1\x306F\x3053\x308C\x304C\x4F55\x3092\x610F\x5473\x3059\x308B\x306E\x304B\x308F\x304B\x308A\x307E\x305B\x3093", XMFLOAT2(10, 512));

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

        wchar_t unicode[256] = {};
        if (!MultiByteToWideChar(437, MB_PRECOMPOSED, ascii, i, unicode, 256))
            wcscpy_s(unicode, L"<ERROR!>\n");

        m_consolasFont->DrawString(m_spriteBatch.get(), unicode, XMFLOAT2(10, 600), Colors::Cyan);
    }

    m_ctrlFont->DrawString(m_spriteBatch.get(), L" !\"\n#$%\n&'()\n*+,-", XMFLOAT2(650, 130), Colors::White, 0.f, XMFLOAT2(0.f, 0.f), 0.5f);

    {
        UINT w, h;

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

        for (UINT x = 0; x < w; x += 100)
        {
            swprintf_s(tmp, L"%u\n", x);
            m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(float(x), float(h - 100)), Colors::Yellow);
        }

        for (UINT y = 0; y < h; y += 100)
        {
            swprintf_s(tmp, L"%u\n", y);
            m_nonproportionalFont->DrawString(m_spriteBatch.get(), tmp, XMFLOAT2(float(w - 100), float(y)), Colors::Red);
        }
    }

#if 0
    m_spriteBatch->End();

    m_spriteBatch->Begin(SpriteSortMode_Deferred, nullptr, nullptr, nullptr, scissorState.Get(), [&]()
    {
        CD3D11_RECT r(640, 20, 740, 38);
        context->RSSetScissorRects(1, &r);
    });

    m_comicFont->DrawString(m_spriteBatch.get(), L"Clipping!", XMFLOAT2(640, 0), Colors::DarkGreen);
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

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        SpriteBatchPipelineStateDescription pd(rtState);

        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
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

    bool success = true;

    // GetDefaultCharacterTest
    if (m_comicFont->GetDefaultCharacter() != 0)
    {
        OutputDebugStringA("FAILED: GetDefaultCharacter\n");
        success = false;
    }

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

     
        auto spriteSheet = m_comicFont->GetSpriteSheet();

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
    if (m_comicFont->GetDefaultCharacter() != '-')
    {
        OutputDebugStringA("FAILED: Get/SetDefaultCharacter\n");
        success = false;
    }

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
        auto spinText = L"Spinning\nlike a cat";

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
    }

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");
}