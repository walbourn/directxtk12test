//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectX Tool Kit - NPREffect
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"
#include "Bezier.h"

#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float row0 = 1.5f;
    constexpr float row1 = -1.5f;

    constexpr float col0 = -4.f;
    constexpr float col1 = -2.f;
    constexpr float col2 = 0.f;
    constexpr float col3 = 2.f;
    constexpr float col4 = 4.f;

    struct TestVertex
    {
        TestVertex(FXMVECTOR iposition, FXMVECTOR inormal, FXMVECTOR itextureCoordinate)
        {
            XMStoreFloat3(&position, iposition);
            XMStoreFloat3(&normal, inormal);
            XMStoreFloat2(&textureCoordinate, itextureCoordinate);
            color = 0xFFFF00FF; // magenta for vertex color testing
        }

        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT2 textureCoordinate;
        XMUBYTE4 color;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;

    private:
        static constexpr unsigned int InputElementCount = 4;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };

    const D3D12_INPUT_ELEMENT_DESC TestVertex::InputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    const D3D12_INPUT_LAYOUT_DESC TestVertex::InputLayout =
    {
        TestVertex::InputElements,
        TestVertex::InputElementCount
    };

    using VertexCollection = std::vector<TestVertex>;
    using IndexCollection = std::vector<uint16_t>;

    #include "../../Src/TeapotData.inc"

    // Tessellates the specified bezier patch.
    void TessellatePatch(VertexCollection& vertices, IndexCollection& indices, TeapotPatch const& patch, FXMVECTOR scale, bool isMirrored)
    {
        constexpr int tessellation = 16;

        // Look up the 16 control points for this patch.
        XMVECTOR controlPoints[16];

        for (int i = 0; i < 16; i++)
        {
            controlPoints[i] = XMVectorMultiply(TeapotControlPoints[patch.indices[i]], scale);
        }

        // Create the index data.
        Bezier::CreatePatchIndices(tessellation, isMirrored, [&](size_t index)
        {
            indices.push_back((uint16_t)(vertices.size() + index));
        });

        // Create the vertex data.
        Bezier::CreatePatchVertices(controlPoints, tessellation, isMirrored, [&](FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate)
        {
            vertices.push_back(TestVertex(position, normal, textureCoordinate));
        });
    }

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
} // anonymous namespace


//--------------------------------------------------------------------------------------

Game::Game() noexcept(false) :
    m_indexCount(0),
    m_vertexBufferView{},
    m_indexBufferView{},
    m_frame(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

#ifdef XBOX
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
    ++m_frame;
}

// Updates the world.
void Game::Update(DX::StepTimer const& /*timer*/)
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

    // Time-based animation
    float time = static_cast<float>(m_timer.GetTotalSeconds());

    float yaw = time * 0.4f;
    float pitch = time * 0.7f;
    float roll = time * 1.1f;

    XMMATRIX world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

    // Setup for teapot drawing.
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //--- NPREffect: Cel shading -----------------------------------------------------------

    // Default cel shading (4 bands).
    m_celEffect->SetWorld(world * XMMatrixTranslation(col0, row0, 0));
    m_celEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with 2 bands.
    m_celEffectBands2->SetWorld(world * XMMatrixTranslation(col1, row0, 0));
    m_celEffectBands2->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with 8 bands.
    m_celEffectBands8->SetWorld(world * XMMatrixTranslation(col2, row0, 0));
    m_celEffectBands8->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading, no specular.
    m_celEffectNoSpecular->SetWorld(world * XMMatrixTranslation(col3, row0, 0));
    m_celEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with vertex color.
    m_celEffectVc->SetWorld(world * XMMatrixTranslation(col4, row0, 0));
    m_celEffectVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- NPREffect: Gooch shading ---------------------------------------------------------

    // Default Gooch shading.
    m_goochEffect->SetWorld(world * XMMatrixTranslation(col0, row1, 0));
    m_goochEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading, no specular.
    m_goochEffectNoSpecular->SetWorld(world * XMMatrixTranslation(col1, row1, 0));
    m_goochEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with vertex color.
    m_goochEffectVc->SetWorld(world * XMMatrixTranslation(col2, row1, 0));
    m_goochEffectVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with custom cool/warm colors.
    m_goochEffectCustom->SetWorld(world * XMMatrixTranslation(col3, row1, 0));
    m_goochEffectCustom->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

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

    m_states = std::make_unique<CommonStates>(device);

    CreateTeapot();

    // Create NPREffect instances
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    XMVECTORF32 blue, red;
#ifdef GAMMA_CORRECT_RENDERING
    blue.v = XMColorSRGBToRGB(Colors::Blue);
    red.v  = XMColorSRGBToRGB(Colors::Red);
#else
    blue.v = Colors::Blue;
    red.v  = Colors::Red;
#endif

    {
        const EffectPipelineStateDescription pdOpaque(
            &TestVertex::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
#ifdef LH_COORDS
            CommonStates::CullClockwise,
#else
            CommonStates::CullCounterClockwise,
#endif
            rtState);

        //--- Cel shading (Mode_Cel) -------------------------------------------------------

        // Default cel shading.
        m_celEffect = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Cel);
        m_celEffect->EnableDefaultLighting();
        m_celEffect->SetDiffuseColor(blue);
        m_celEffect->SetCelShaderBands(4);

        // Cel shading with 2 bands.
        m_celEffectBands2 = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectBands2->EnableDefaultLighting();
        m_celEffectBands2->SetDiffuseColor(blue);
        m_celEffectBands2->SetCelShaderBands(2);

        // Cel shading with 8 bands.
        m_celEffectBands8 = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectBands8->EnableDefaultLighting();
        m_celEffectBands8->SetDiffuseColor(blue);
        m_celEffectBands8->SetCelShaderBands(8);

        // Cel shading, no specular.
        m_celEffectNoSpecular = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectNoSpecular->EnableDefaultLighting();
        m_celEffectNoSpecular->SetDiffuseColor(blue);
        m_celEffectNoSpecular->SetCelShaderBands(4);
        m_celEffectNoSpecular->DisableSpecular();

        // Cel shading with vertex color.
        m_celEffectVc = std::make_unique<NPREffect>(device, EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectVc->EnableDefaultLighting();
        m_celEffectVc->SetCelShaderBands(4);

        //--- Gooch shading (Mode_Gooch) ---------------------------------------------------

        // Default Gooch shading.
        m_goochEffect = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffect->EnableDefaultLighting();
        m_goochEffect->SetDiffuseColor(red);

        // Gooch shading, no specular.
        m_goochEffectNoSpecular = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectNoSpecular->EnableDefaultLighting();
        m_goochEffectNoSpecular->SetDiffuseColor(red);
        m_goochEffectNoSpecular->DisableSpecular();

        // Gooch shading with vertex color.
        m_goochEffectVc = std::make_unique<NPREffect>(device, EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectVc->EnableDefaultLighting();

        // Gooch shading with custom cool/warm colors.
        m_goochEffectCustom = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectCustom->EnableDefaultLighting();
        m_goochEffectCustom->SetDiffuseColor(red);
        m_goochEffectCustom->SetGoochCoolColor(Colors::Cyan, 0.4f);
        m_goochEffectCustom->SetGoochWarmColor(Colors::Yellow, 0.4f);
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 6.f, 0.f } } };

    const auto size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 15);
#else
    XMMATRIX view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 15);
#endif

#ifdef UWP
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        projection *= orient;
    }
#endif

    m_celEffect->SetView(view);
    m_celEffectBands2->SetView(view);
    m_celEffectBands8->SetView(view);
    m_celEffectNoSpecular->SetView(view);
    m_celEffectVc->SetView(view);

    m_goochEffect->SetView(view);
    m_goochEffectNoSpecular->SetView(view);
    m_goochEffectVc->SetView(view);
    m_goochEffectCustom->SetView(view);

    m_celEffect->SetProjection(projection);
    m_celEffectBands2->SetProjection(projection);
    m_celEffectBands8->SetProjection(projection);
    m_celEffectNoSpecular->SetProjection(projection);
    m_celEffectVc->SetProjection(projection);

    m_goochEffect->SetProjection(projection);
    m_goochEffectNoSpecular->SetProjection(projection);
    m_goochEffectVc->SetProjection(projection);
    m_goochEffectCustom->SetProjection(projection);
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_celEffect.reset();
    m_celEffectBands2.reset();
    m_celEffectBands8.reset();
    m_celEffectNoSpecular.reset();
    m_celEffectVc.reset();

    m_goochEffect.reset();
    m_goochEffectNoSpecular.reset();
    m_goochEffectVc.reset();
    m_goochEffectCustom.reset();

    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();

    m_states.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion

// Creates a teapot primitive with test input layout.
void Game::CreateTeapot()
{
    VertexCollection vertices;
    IndexCollection indices;

    XMVECTOR negateXZ = XMVectorMultiply(g_XMNegateX, g_XMNegateZ);

    for (size_t i = 0; i < sizeof(TeapotPatches) / sizeof(TeapotPatches[0]); i++)
    {
        TeapotPatch const& patch = TeapotPatches[i];

        // Because the teapot is symmetrical from left to right, we only store
        // data for one side, then tessellate each patch twice, mirroring in X.
        TessellatePatch(vertices, indices, patch, g_XMOne, false);
        TessellatePatch(vertices, indices, patch, g_XMNegateX, true);

        if (patch.mirrorZ)
        {
            // Some parts of the teapot (the body, lid, and rim, but not the
            // handle or spout) are also symmetrical from front to back, so
            // we tessellate them four times, mirroring in Z as well as X.
            TessellatePatch(vertices, indices, patch, g_XMNegateZ, true);
            TessellatePatch(vertices, indices, patch, negateXZ, false);
        }
    }

    // Create the D3D buffers.
    if (vertices.size() >= USHRT_MAX)
        throw std::runtime_error("Too many vertices for 16-bit index buffer");

    // Vertex data
    auto verts = reinterpret_cast<const uint8_t*>(vertices.data());
    size_t vertSizeBytes = vertices.size() * sizeof(TestVertex);

    m_vertexBuffer = GraphicsMemory::Get().Allocate(vertSizeBytes);
    memcpy(m_vertexBuffer.Memory(), verts, vertSizeBytes);

    // Index data
    auto ind = reinterpret_cast<const uint8_t*>(indices.data());
    size_t indSizeBytes = indices.size() * sizeof(uint16_t);

    m_indexBuffer = GraphicsMemory::Get().Allocate(indSizeBytes);
    memcpy(m_indexBuffer.Memory(), ind, indSizeBytes);

    // Record index count for draw
    m_indexCount = static_cast<UINT>(indices.size());

    // Create views
    m_vertexBufferView.BufferLocation = m_vertexBuffer.GpuAddress();
    m_vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(VertexCollection::value_type));
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexBuffer.Size());

    m_indexBufferView.BufferLocation = m_indexBuffer.GpuAddress();
    m_indexBufferView.SizeInBytes = static_cast<UINT>(m_indexBuffer.Size());
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
}
