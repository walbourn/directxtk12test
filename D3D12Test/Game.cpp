//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for basic Direct3D 12 support
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

//--------------------------------------------------------------------------------------
// As of DirectXMath 3.13, these types are is_nothrow_copy/move_assignable

// VertexPosition
static_assert(std::is_nothrow_default_constructible<VertexPosition>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPosition>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPosition>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPosition>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPosition>::value, "Move Assign.");

// VertexPositionColor
static_assert(std::is_nothrow_default_constructible<VertexPositionColor>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionColor>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionColor>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionColor>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionColor>::value, "Move Assign.");

// VertexPositionTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionTexture>::value, "Move Assign.");

// VertexPositionDualTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionDualTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionDualTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionDualTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionDualTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionDualTexture>::value, "Move Assign.");

// VertexPositionNormal
static_assert(std::is_nothrow_default_constructible<VertexPositionNormal>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormal>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormal>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormal>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormal>::value, "Move Assign.");

// VertexPositionColorTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionColorTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionColorTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionColorTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionColorTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionColorTexture>::value, "Move Assign.");

// VertexPositionNormalColor
static_assert(std::is_nothrow_default_constructible<VertexPositionNormalColor>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormalColor>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormalColor>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormalColor>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormalColor>::value, "Move Assign.");

// VertexPositionNormalTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionNormalTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormalTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormalTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormalTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormalTexture>::value, "Move Assign.");

// VertexPositionNormalColorTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionNormalColorTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormalColorTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormalColorTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormalColorTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormalColorTexture>::value, "Move Assign.");

//--------------------------------------------------------------------------------------

Game::Game() noexcept(false)
{
#ifdef GAMMA_CORRECT_RENDERING
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

#if defined(_XBOX_ONE) && defined(_TITLE)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD
        );
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox
        );
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
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

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

    // Point
    {
        ScopedPixEvent pix(commandList, 0, L"Points");
        m_effectPoint->Apply(commandList);

        m_batch->Begin(commandList);

        {
            VertexPositionColor points[]
            {
                { Vector3(-0.75f, -0.75f, 0.5f), red },
                { Vector3(-0.75f, -0.5f,  0.5f), green },
                { Vector3(-0.75f, -0.25f, 0.5f), blue },
                { Vector3(-0.75f,  0.0f,  0.5f), yellow },
                { Vector3(-0.75f,  0.25f, 0.5f), magenta },
                { Vector3(-0.75f,  0.5f,  0.5f), cyan },
                { Vector3(-0.75f,  0.75f, 0.5f), Colors::White },
            };

            m_batch->Draw(D3D_PRIMITIVE_TOPOLOGY_POINTLIST, points, _countof(points));
        }

        m_batch->End();
    }

    // Lines
    {
        ScopedPixEvent pix(commandList, 0, L"Lines");
        m_effectLine->Apply(commandList);

        m_batch->Begin(commandList);

        {
            VertexPositionColor lines[] =
            {
                { Vector3(-0.75f, -0.85f, 0.5f), red },{ Vector3(0.75f, -0.85f, 0.5f), dred },
                { Vector3(-0.75f, -0.90f, 0.5f), green },{ Vector3(0.75f, -0.90f, 0.5f), dgreen },
                { Vector3(-0.75f, -0.95f, 0.5f), blue },{ Vector3(0.75f, -0.95f, 0.5f), dblue },
            };

            m_batch->DrawLine(lines[0], lines[1]);
            m_batch->DrawLine(lines[2], lines[3]);
            m_batch->DrawLine(lines[4], lines[5]);
        }

        m_batch->End();
    }

    // Triangle
    {
        ScopedPixEvent pix(commandList, 0, L"Triangles");
        m_effectTri->Apply(commandList);

        m_batch->Begin(commandList);

        {
            VertexPositionColor v1(Vector3(0.f, 0.5f, 0.5f), red);
            VertexPositionColor v2(Vector3(0.5f, -0.5f, 0.5f), green);
            VertexPositionColor v3(Vector3(-0.5f, -0.5f, 0.5f), blue);

            m_batch->DrawTriangle(v1, v2, v3);
        }

        // Quad (same type as triangle)
        {
            VertexPositionColor quad[] =
            {
                { Vector3(0.75f, 0.75f, 0.5), gray },
                { Vector3(0.95f, 0.75f, 0.5), gray },
                { Vector3(0.95f, -0.75f, 0.5), dgray },
                { Vector3(0.75f, -0.75f, 0.5), dgray },
            };

            m_batch->DrawQuad(quad[0], quad[1], quad[2], quad[3]);
        }

        m_batch->End();
    }

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
    SetDebugObjectName(m_deviceResources->GetRenderTarget(), L"RenderTarget");
    SetDebugObjectName(m_deviceResources->GetDepthStencil(), "DepthStencil");
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_test1.Reset();
    m_test2.Reset();
    m_test3.Reset();

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
#endif
#pragma endregion

namespace
{
    inline bool CheckIsPowerOf2(size_t x) noexcept
    {
        if (!x)
            return false;

        return (ceil(log2(x)) == float(log2(x)));
    }

    inline uint64_t CheckAlignUp(uint64_t size, size_t alignment) noexcept
    {
        return ((size + alignment - 1) / alignment) * alignment;
    }

    inline uint64_t CheckAlignDown(uint64_t size, size_t alignment) noexcept
    {
        return (size / alignment) * alignment;
    }
}

void Game::UnitTests()
{
    bool success = true;
    OutputDebugStringA("*********** UINT TESTS BEGIN ***************\n");

    std::random_device rd;
    std::default_random_engine generator(rd());

    {
        for (size_t j = 0; j < 0x20000; ++j)
        {
            if (IsPowerOf2(j) != CheckIsPowerOf2(j))
            {
                OutputDebugStringA("ERROR: Failed IsPowerOf2 tests\n");
                success = false;
            }
        }
    }

    // uint32_t
    {
        std::uniform_int_distribution<uint32_t> dist(1, UINT16_MAX);
        for (size_t j = 1; j < 0x20000; j <<= 1)
        {
            if (!IsPowerOf2(j))
            {
                OutputDebugStringA("ERROR: Failed IsPowerOf2 Align(32)\n");
                success = false;
            }

            for (size_t k = 0; k < 20000; k++)
            {
                uint32_t value = dist(generator);
                uint32_t up = AlignUp(value, j);
                uint32_t down = AlignDown(value, j);
                auto upCheck = static_cast<uint32_t>(CheckAlignUp(value, j));
                auto downCheck = static_cast<uint32_t>(CheckAlignDown(value, j));

                if (!up)
                {
                    OutputDebugStringA("ERROR: Failed AlignUp(32) tests\n");
                    success = false;
                }
                else if (!down && value > j)
                {
                    OutputDebugStringA("ERROR: Failed AlignDown(32) tests\n");
                    success = false;
                }
                else if (up < down)
                {
                    OutputDebugStringA("ERROR: Failed Align(32) tests\n");
                    success = false;
                }
                else if (value > up)
                {
                    OutputDebugStringA("ERROR: Failed AlignUp(32) tests\n");
                    success = false;
                }
                else if (value < down)
                {
                    OutputDebugStringA("ERROR: Failed AlignDown(32) tests\n");
                    success = false;
                }
                else if (up != upCheck)
                {
                    OutputDebugStringA("ERROR: Failed AlignUp(32) tests\n");
                    success = false;
                }
                else if (down != downCheck)
                {
                    OutputDebugStringA("ERROR: Failed AlignDown(32) tests\n");
                    success = false;
                }
            }
        }
    }

    // uint64_t
    {
        std::uniform_int_distribution<uint64_t> dist(1, UINT32_MAX);
        for (size_t j = 1; j < 0x20000; j <<= 1)
        {
            if (!IsPowerOf2(j))
            {
                OutputDebugStringA("ERROR: Failed IsPowerOf2 Align(64)\n");
                success = false;
            }

            for (size_t k = 0; k < 20000; k++)
            {
                uint64_t value = dist(generator);
                uint64_t up = AlignUp(value, j);
                uint64_t down = AlignDown(value, j);
                uint64_t upCheck = CheckAlignUp(value, j);
                uint64_t downCheck = CheckAlignDown(value, j);

                if (!up)
                {
                    OutputDebugStringA("ERROR: Failed AlignUp(64) tests\n");
                    success = false;
                }
                else if (!down && value > j)
                {
                    OutputDebugStringA("ERROR: Failed AlignDown(64) tests\n");
                    success = false;
                }
                else if (up < down)
                {
                    OutputDebugStringA("ERROR: Failed Align(64) tests\n");
                    success = false;
                }
                else if (value > up)
                {
                    OutputDebugStringA("ERROR: Failed AlignUp(64) tests\n");
                    success = false;
                }
                else if (value < down)
                {
                    OutputDebugStringA("ERROR: Failed AlignDown(64) tests\n");
                    success = false;
                }
                else if (up != upCheck)
                {
                    OutputDebugStringA("ERROR: Failed AlignUp(64) tests\n");
                    success = false;
                }
                else if (down != downCheck)
                {
                    OutputDebugStringA("ERROR: Failed AlignDown(64) tests\n");
                    success = false;
                }
            }
        }
    }

#if defined(__cplusplus_winrt)
    // SimpleMath interop tests for Windows Runtime types
    Rectangle test1(10, 20, 50, 100);

    Windows::Foundation::Rect test2 = test1;
    if (test1.x != long(test2.X)
        && test1.y != long(test2.Y)
        && test1.width != long(test2.Width)
        && test1.height != long(test2.Height))
    {
        OutputDebugStringA("SimpleMath::Rectangle operator test A failed!");
        success = false;
    }
#endif

    auto device = m_deviceResources->GetD3DDevice();

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    // CreateStaticBuffer (BufferHelpers.h)
    {
        static const VertexPositionColor s_vertexData[3] =
        {
            { XMFLOAT3{ 0.0f,   0.5f,  0.5f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },  // Top / Red
            { XMFLOAT3{ 0.5f,  -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },  // Right / Green
            { XMFLOAT3{ -0.5f, -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }   // Left / Blue
        };

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            s_vertexData, _countof(s_vertexData), sizeof(VertexPositionColor),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            m_test1.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateStaticBuffer(1) test\n");
            success = false;
        }

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            s_vertexData, _countof(s_vertexData),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            m_test2.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateStaticBuffer(2) test\n");
            success = false;
        }

        std::vector<VertexPositionColor> verts(s_vertexData, s_vertexData + _countof(s_vertexData));

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            verts,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            m_test3.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateStaticBuffer(3) test\n");
            success = false;
        }
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");
}
