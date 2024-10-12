//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for basic Direct3D 12 support
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

static_assert(std::is_nothrow_move_constructible<CommonStates>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<CommonStates>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<GraphicsMemory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GraphicsMemory>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<PrimitiveBatch<VertexPositionColor>>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<PrimitiveBatch<VertexPositionColor>>::value, "Move Assign.");

//--------------------------------------------------------------------------------------

namespace
{
#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
}

// Constructor.
Game::Game() noexcept(false)
{
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
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
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
    m_gamePad = std::make_unique<GamePad>();
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
#ifdef _GAMING_XBOX
    m_deviceResources->WaitForOrigin();
#endif

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
    #if defined(_PIX_H_) || defined(_PIX3_H_)
        ScopedPixEvent pix(commandList, 0, L"Points");
    #endif
        m_effectPoint->Apply(commandList);

        m_batch->Begin(commandList);

        {
            Vertex points[]
            {
                { Vector3(-0.75f, -0.75f, 0.5f), red },
                { Vector3(-0.75f, -0.5f,  0.5f), green },
                { Vector3(-0.75f, -0.25f, 0.5f), blue },
                { Vector3(-0.75f,  0.0f,  0.5f), yellow },
                { Vector3(-0.75f,  0.25f, 0.5f), magenta },
                { Vector3(-0.75f,  0.5f,  0.5f), cyan },
                { Vector3(-0.75f,  0.75f, 0.5f), Colors::White },
            };

            m_batch->Draw(D3D_PRIMITIVE_TOPOLOGY_POINTLIST, points, static_cast<UINT>(std::size(points)));
        }

        m_batch->End();
    }

    // Lines
    {
    #if defined(_PIX_H_) || defined(_PIX3_H_)
        ScopedPixEvent pix(commandList, 0, L"Lines");
    #endif
        m_effectLine->Apply(commandList);

        m_batch->Begin(commandList);

        {
            Vertex lines[] =
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
    #if defined(_PIX_H_) || defined(_PIX3_H_)
        ScopedPixEvent pix(commandList, 0, L"Triangles");
    #endif
        m_effectTri->Apply(commandList);

        m_batch->Begin(commandList);

        {
            Vertex v1(Vector3(0.f, 0.5f, 0.5f), red);
            Vertex v2(Vector3(0.5f, -0.5f, 0.5f), green);
            Vertex v3(Vector3(-0.5f, -0.5f, 0.5f), blue);

            m_batch->DrawTriangle(v1, v2, v3);
        }

        // Quad (same type as triangle)
        {
            Vertex quad[] =
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
    std::ignore = m_graphicsMemory->GetStatistics();

    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto const rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto const dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, c_clearColor, 0, nullptr);
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

    m_batch = std::make_unique<PrimitiveBatch<Vertex>>(device);

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
            &Vertex::InputLayout,
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

    m_states = std::make_unique<CommonStates>(device);

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, 3);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    SetDebugObjectName(m_deviceResources->GetRenderTarget(), L"RenderTarget");
    SetDebugObjectName(m_deviceResources->GetDepthStencil(), "DepthStencil");
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_test1.Reset();
    m_test2.Reset();
    m_test3.Reset();
    m_test4.Reset();
    m_test5.Reset();
    m_test6.Reset();
    m_test7.Reset();
    m_test8.Reset();
    m_test9.Reset();
    m_test10.Reset();

    m_batch.reset();
    m_effectTri.reset();
    m_effectPoint.reset();
    m_effectLine.reset();
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

    bool TestBlendState(const D3D12_BLEND_DESC& state, const RenderTargetState& rtState, _In_ ID3D12Device* device)
    {
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            state,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        try
        {
            auto effect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
            if (!effect)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    bool TestDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& state, const RenderTargetState& rtState, _In_ ID3D12Device* device)
    {
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            CommonStates::Opaque,
            state,
            CommonStates::CullNone,
            rtState);

        try
        {
            auto effect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
            if (!effect)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    bool TestRasterizerState(const D3D12_RASTERIZER_DESC& state, const RenderTargetState& rtState, _In_ ID3D12Device* device)
    {
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            state,
            rtState);

        try
        {
            auto effect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
            if (!effect)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return true;
    }
}

template<class T>
inline bool TestVertexType(const RenderTargetState& rtState, _In_ ID3D12Device* device)
{
    if (T::InputLayout.NumElements == 0
        || T::InputLayout.pInputElementDescs == nullptr)
        return false;

    if (T::InputLayout.NumElements > D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)
        return false;

    if (_stricmp(T::InputLayout.pInputElementDescs[0].SemanticName, "SV_Position") != 0)
    {
        return false;
    }

    for (size_t j = 0; j < T::InputLayout.NumElements; ++j)
    {
        if (T::InputLayout.pInputElementDescs[0].SemanticName == nullptr)
            return false;
    }

    {
        EffectPipelineStateDescription pd(
            &T::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        try
        {
            auto effect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
            if (!effect)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }
    }

    return true;
}

void Game::UnitTests()
{
    bool success = true;
    OutputDebugStringA("*********** UNIT TESTS BEGIN ***************\n");

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

    // CreateUploadBuffer (BufferHelpers.h)
    {
        static const VertexPositionColor s_vertexData[3] =
        {
            { XMFLOAT3{ 0.0f,   0.5f,  0.5f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },  // Top / Red
            { XMFLOAT3{ 0.5f,  -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },  // Right / Green
            { XMFLOAT3{ -0.5f, -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }   // Left / Blue
        };

        if (FAILED(CreateUploadBuffer(device,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            m_test1.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateUploadBuffer(1) test\n");
            success = false;
        }

        CreateBufferShaderResourceView(device, m_test1.Get(),
            m_resourceDescriptors->GetFirstCpuHandle(),
            sizeof(float));

        if (FAILED(CreateUploadBuffer(device,
            s_vertexData, std::size(s_vertexData),
            m_test2.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateUploadBuffer(2) test\n");
            success = false;
        }

        std::vector<VertexPositionColor> verts(s_vertexData, s_vertexData + std::size(s_vertexData));

        if (FAILED(CreateUploadBuffer(device,
            verts,
            m_test3.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateUploadBuffer(3) test\n");
            success = false;
        }

        // DSR
        if (FAILED(CreateUploadBuffer(device,
            verts,
            m_test4.ReleaseAndGetAddressOf(),
            D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)))
        {
            OutputDebugStringA("ERROR: Failed CreateUploadBuffer(DSR) test\n");
            success = false;
        }

        // No data
        if (FAILED(CreateUploadBuffer(device,
            nullptr, 14, sizeof(VertexPositionDualTexture),
            m_test5.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateUploadBuffer(no data) test\n");
            success = false;
        }
    }

    // CreateUAVBuffer
    if (FAILED(CreateUAVBuffer(device,
        sizeof(VertexPositionNormalColorTexture),
        m_test6.ReleaseAndGetAddressOf())))
    {
        OutputDebugStringA("ERROR: Failed CreateUAVBuffer test\n");
        success = false;
    }

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
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            m_test7.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateStaticBuffer(1) test\n");
            success = false;
        }

        CreateBufferShaderResourceView(device, m_test7.Get(),
            m_resourceDescriptors->GetCpuHandle(1),
            sizeof(float));

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            s_vertexData, std::size(s_vertexData),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            m_test8.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateStaticBuffer(2) test\n");
            success = false;
        }

        std::vector<VertexPositionColor> verts(s_vertexData, s_vertexData + std::size(s_vertexData));

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            verts,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            m_test9.ReleaseAndGetAddressOf())))
        {
            OutputDebugStringA("ERROR: Failed CreateStaticBuffer(3) test\n");
            success = false;
        }

        // UAV
        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            verts,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            m_test10.ReleaseAndGetAddressOf(),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)))
        {
            OutputDebugStringA("ERROR: Failed CreateStaticBuffer(UAV) test\n");
            success = false;
        }

        CreateBufferUnorderedAccessView(device, m_test10.Get(),
            m_resourceDescriptors->GetCpuHandle(2),
            sizeof(VertexPositionColor));
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    // CommonStates.h
    {
        const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
            m_deviceResources->GetDepthBufferFormat());

        if (!TestBlendState(CommonStates::Opaque, rtState, device)
            || !TestBlendState(CommonStates::AlphaBlend, rtState, device)
            || !TestBlendState(CommonStates::Additive, rtState, device)
            || !TestBlendState(CommonStates::NonPremultiplied, rtState, device))
        {
            OutputDebugStringA("ERROR: Failed CommonStates blend state tests\n");
            success = false;
        }

        if (!TestDepthStencilState(CommonStates::DepthNone, rtState, device)
            || !TestDepthStencilState(CommonStates::DepthDefault, rtState, device)
            || !TestDepthStencilState(CommonStates::DepthRead, rtState, device)
            || !TestDepthStencilState(CommonStates::DepthReverseZ, rtState, device)
            || !TestDepthStencilState(CommonStates::DepthReadReverseZ, rtState, device))
        {
            OutputDebugStringA("ERROR: Failed CommonStates depth/stencil state tests\n");
            success = false;
        }

        if (!TestRasterizerState(CommonStates::CullNone, rtState, device)
            || !TestRasterizerState(CommonStates::CullClockwise, rtState, device)
            || !TestRasterizerState(CommonStates::CullCounterClockwise, rtState, device)
            || !TestRasterizerState(CommonStates::Wireframe, rtState, device))
        {
            OutputDebugStringA("ERROR: Failed CommonStates rasterizer state tests\n");
            success = false;
        }

        {
            auto desc1 = CommonStates::StaticPointWrap(2);
            auto desc2 = CommonStates::StaticPointClamp(3);
            auto desc3 = CommonStates::StaticLinearWrap(1);
            auto desc4 = CommonStates::StaticLinearClamp(8);
            auto desc5 = CommonStates::StaticAnisotropicWrap(4);
            auto desc6 = CommonStates::StaticAnisotropicClamp(6);
            if (desc1.ShaderRegister != 2
                || desc2.ShaderRegister != 3
                || desc3.ShaderRegister != 1
                || desc4.ShaderRegister != 8
                || desc5.ShaderRegister != 4
                || desc6.ShaderRegister != 6)
            {
                OutputDebugStringA("ERROR: Failed CommonStates static sampler state tests\n");
                    success = false;
            }
        }

        if (m_states->Heap() == nullptr
            || m_states->PointWrap().ptr == 0
            || m_states->PointClamp().ptr == 0
            || m_states->LinearWrap().ptr == 0
            || m_states->LinearClamp().ptr == 0
            || m_states->AnisotropicWrap().ptr == 0
            || m_states->AnisotropicClamp().ptr == 0)
        {
            OutputDebugStringA("ERROR: Failed CommonStates heap sampler state tests\n");
            success = false;
        }
    }

    // VertexTypes.h
    {
        const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
            m_deviceResources->GetDepthBufferFormat());

        if (!TestVertexType<VertexPosition>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPosition tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionColor>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionColor tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionTexture>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionTexture tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionDualTexture>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionDualTexture tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionNormal>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionNormal tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionColorTexture>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionColorTexture tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionNormalColor>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionNormalColor tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionNormalTexture>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionNormalTexture tests\n");
            success = false;
        }

        if (!TestVertexType<VertexPositionNormalColorTexture>(rtState, device))
        {
            OutputDebugStringA("ERROR: Failed VertexPositionNormalColorTexture tests\n");
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
