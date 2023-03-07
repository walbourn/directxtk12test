//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Geometric Primitives
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#define GAMMA_CORRECT_RENDERING
#define USE_COPY_QUEUE
#define USE_COMPUTE_QUEUE

// Build for LH vs. RH coords
//#define LH_COORDS

#define REVERSEZ

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float rowtop = 4.f;
    constexpr float row0 = 2.7f;
    constexpr float row1 = 1.f;
    constexpr float row2 = -0.7f;
    constexpr float row3 = -2.5f;

    constexpr float col0 = -7.5f;
    constexpr float col1 = -5.75f;
    constexpr float col2 = -4.25f;
    constexpr float col3 = -2.7f;
    constexpr float col4 = -1.25f;
    constexpr float col5 = 0.f;
    constexpr float col6 = 1.25f;
    constexpr float col7 = 2.5f;
    constexpr float col8 = 4.25f;
    constexpr float col9 = 5.75f;
    constexpr float col10 = 7.5f;

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
}

static_assert(std::is_nothrow_move_constructible<GeometricPrimitive>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GeometricPrimitive>::value, "Move Assign.");

Game::Game() noexcept(false) :
    m_instanceCount(0),
    m_spinning(true),
    m_firstFrame(false),
    m_pitch(0),
    m_yaw(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

    unsigned int deviceOptions = 0;
#ifdef REVERSEZ
    deviceOptions |= DX::DeviceResources::c_ReverseDepth;
#endif

#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2,
        deviceOptions | DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        deviceOptions | DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat,
        DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        deviceOptions
        );
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

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitGame();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED)
        {
            m_spinning = !m_spinning;
        }

        if (pad.IsLeftStickPressed())
        {
            m_spinning = false;
            m_yaw = m_pitch = 0.f;
        }
        else
        {
            m_yaw += pad.thumbSticks.leftX * 0.1f;
            m_pitch -= pad.thumbSticks.leftY * 0.1f;
        }
    }
    else
    {
        m_gamePadButtons.Reset();

        if (kb.A || kb.D)
        {
            m_spinning = false;
            m_yaw += (kb.D ? 0.1f : -0.1f);
        }

        if (kb.W || kb.S)
        {
            m_spinning = false;
            m_pitch += (kb.W ? 0.1f : -0.1f);
        }

        if (kb.Home)
        {
            m_spinning = false;
            m_yaw = m_pitch = 0.f;
        }
    }

    if (m_yaw > XM_PI)
    {
        m_yaw -= XM_PI * 2.f;
    }
    else if (m_yaw < -XM_PI)
    {
        m_yaw += XM_PI * 2.f;
    }

    if (kb.Escape)
    {
        ExitGame();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        m_spinning = !m_spinning;
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

    auto time = static_cast<float>(m_timer.GetTotalSeconds());

    float alphaFade = (sin(time * 2) + 1) / 2;

    if (alphaFade >= 1)
        alphaFade = 1 - FLT_EPSILON;

    float yaw = time * 0.4f;
    float pitch = time * 0.7f;
    float roll = time * 1.1f;

    XMMATRIX world;

    if (m_spinning)
    {
        world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    }
    else
    {
        world = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0);
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    if (m_firstFrame)
    {
        //
        // This is not strictly needed on Windows due to common state promotion, but this behavior is optional on Xbox
        //

        // Copy queue resources are left in D3D12_RESOURCE_STATE_COPY_DEST state
        #ifdef USE_COPY_QUEUE
        m_torus->Transition(commandList,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
        #endif

        // Compute queue IBs are in D3D12_RESOURCE_STATE_COPY_DEST state
        #ifdef USE_COMPUTE_QUEUE
        m_teapot->Transition(commandList,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
        #endif

        m_firstFrame = false;
    }

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    XMVECTORF32 red, green, blue, yellow, cyan, magenta, cornflower, lime, gray;
#ifdef GAMMA_CORRECT_RENDERING
    red.v = XMColorSRGBToRGB(Colors::Red);
    green.v = XMColorSRGBToRGB(Colors::Green);
    blue.v = XMColorSRGBToRGB(Colors::Blue);
    yellow.v = XMColorSRGBToRGB(Colors::Yellow);
    cyan.v = XMColorSRGBToRGB(Colors::Cyan);
    magenta.v = XMColorSRGBToRGB(Colors::Magenta);
    cornflower.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
    lime.v = XMColorSRGBToRGB(Colors::Lime);
    gray.v = XMColorSRGBToRGB(Colors::Gray);
#else
    red.v = Colors::Red;
    green.v = Colors::Green;
    blue.v = Colors::Blue;
    yellow.v = Colors::Yellow;
    cyan.v = Colors::Cyan;
    magenta.v = Colors::Magenta;
    cornflower.v = Colors::CornflowerBlue;
    lime.v = Colors::Lime;
    gray.v = Colors::Gray;
#endif
    SimpleMath::Vector4 white = Colors::White.v;

    //--- Draw shapes ----------------------------------------------------------------------
    m_effect->SetWorld(world * XMMatrixTranslation(col0, row0, 0));
    m_effect->SetDiffuseColor(Colors::White);
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col1, row0, 0));
    m_effect->SetDiffuseColor(red);
    m_effect->Apply(commandList);
    m_sphere->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col2, row0, 0));
    m_effect->SetDiffuseColor(green);
    m_effect->Apply(commandList);
    m_geosphere->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col3, row0, 0));
    m_effect->SetDiffuseColor(lime);
    m_effect->Apply(commandList);
    m_cylinder->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col4, row0, 0));
    m_effect->SetDiffuseColor(yellow);
    m_effect->Apply(commandList);
    m_cone->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col5, row0, 0));
    m_effect->SetDiffuseColor(blue);
    m_effect->Apply(commandList);
    m_torus->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col6, row0, 0));
    m_effect->SetDiffuseColor(cornflower);
    m_effect->Apply(commandList);
    m_teapot->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col7, row0, 0));
    m_effect->SetDiffuseColor(red);
    m_effect->Apply(commandList);
    m_tetra->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col8, row0, 0));
    m_effect->SetDiffuseColor(lime);
    m_effect->Apply(commandList);
    m_octa->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col9, row0, 0));
    m_effect->SetDiffuseColor(blue);
    m_effect->Apply(commandList);
    m_dodec->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col10, row0, 0));
    m_effect->SetDiffuseColor(cyan);
    m_effect->Apply(commandList);
    m_iso->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col8, row3, 0));
    m_effect->SetDiffuseColor(magenta);
    m_effect->Apply(commandList);
    m_box->Draw(commandList);

    //--- Draw textured shapes -------------------------------------------------------------
    m_effectTexture->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::RefTexture), m_states->AnisotropicWrap());
    m_effectTexture->SetWorld(world * XMMatrixTranslation(col0, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::White);
    m_effectTexture->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col1, row1, 0));
    m_effectTexture->SetDiffuseColor(red);
    m_effectTexture->Apply(commandList);
    m_sphere->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col2, row1, 0));
    m_effectTexture->SetDiffuseColor(green);
    m_effectTexture->Apply(commandList);
    m_geosphere->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col3, row1, 0));
    m_effectTexture->SetDiffuseColor(lime);
    m_effectTexture->Apply(commandList);
    m_cylinder->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col4, row1, 0));
    m_effectTexture->SetDiffuseColor(yellow);
    m_effectTexture->Apply(commandList);
    m_cone->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col5, row1, 0));
    m_effectTexture->SetDiffuseColor(blue);
    m_effectTexture->Apply(commandList);
    m_torus->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col6, row1, 0));
    m_effectTexture->SetDiffuseColor(cornflower);
    m_effectTexture->Apply(commandList);
    m_teapot->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col7, row1, 0));
    m_effectTexture->SetDiffuseColor(red);
    m_effectTexture->Apply(commandList);
    m_tetra->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col8, row1, 0));
    m_effectTexture->SetDiffuseColor(lime);
    m_effectTexture->Apply(commandList);
    m_octa->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col9, row1, 0));
    m_effectTexture->SetDiffuseColor(blue);
    m_effectTexture->Apply(commandList);
    m_dodec->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col10, row1, 0));
    m_effectTexture->SetDiffuseColor(cyan);
    m_effectTexture->Apply(commandList);
    m_iso->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col9, row3, 0));
    m_effectTexture->SetDiffuseColor(magenta);
    m_effectTexture->Apply(commandList);
    m_box->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col7, row3, 0));
    m_effectTexture->SetDiffuseColor(Colors::White);
    m_effectTexture->Apply(commandList);
    m_customBox->Draw(commandList);

    //--- Draw shapes in wireframe ---------------------------------------------------------
    m_effectWireframe->SetDiffuseColor(gray);
    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col0, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col1, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_sphere->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col2, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_geosphere->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col3, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_cylinder->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col4, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_cone->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col5, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_torus->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col6, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_teapot->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col7, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_tetra->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col8, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_octa->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col9, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_dodec->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col10, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_iso->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col10, row3, 0));
    m_effectWireframe->Apply(commandList);
    m_box->Draw(commandList);

    //--- Draw shapes with alpha blending --------------------------------------------------
    m_effectAlpha->SetWorld(world * XMMatrixTranslation(col0, row3, 0));
    m_effectAlpha->SetDiffuseColor(white * alphaFade);
    m_effectAlpha->SetAlpha(alphaFade);
    m_effectAlpha->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectPMAlphaTexture->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat), m_states->AnisotropicWrap());
    m_effectPMAlphaTexture->SetWorld(world * XMMatrixTranslation(col1, row3, 0));
    m_effectPMAlphaTexture->SetDiffuseColor(white * alphaFade);
    m_effectPMAlphaTexture->SetAlpha(alphaFade);
    m_effectPMAlphaTexture->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectAlphaTexture->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat), m_states->AnisotropicWrap());
    m_effectAlphaTexture->SetWorld(world * XMMatrixTranslation(col2, row3, 0));
    m_effectAlphaTexture->SetDiffuseColor(white * alphaFade);
    m_effectAlphaTexture->SetAlpha(alphaFade);
    m_effectAlphaTexture->Apply(commandList);
    m_cube->Draw(commandList);

    //--- Draw dynamic light ---------------------------------------------------------------
    XMVECTOR quat = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
    XMVECTOR dir = XMVector3Rotate(g_XMOne, quat);
    m_effectLights->SetLightDirection(0, dir);
    m_effectLights->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo), m_states->AnisotropicWrap());
    m_effectLights->SetWorld(XMMatrixTranslation(col3, row3, 0));
    m_effectLights->Apply(commandList);
    m_cube->Draw(commandList);

    //--- Draw dynamic light and fog -------------------------------------------------------
    XMMATRIX fbworld = XMMatrixTranslation(0, 0, cos(time) * 2.f);
    m_effectFog->SetWorld(fbworld  * XMMatrixTranslation(col5, row3, 0));
    m_effectFog->SetLightDirection(0, dir);
    m_effectFog->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo), m_states->AnisotropicWrap());
    m_effectFog->Apply(commandList);
    m_cube->Draw(commandList);

    //--- Draw shapes with instancing ------------------------------------------------------
    {
        size_t j = 0;
        for (float x = -8.f; x <= 8.f; x += 3.f)
        {
            XMMATRIX m = world * XMMatrixTranslation(x, 0.f, cos(time + float(j) * XM_PIDIV4));
            XMStoreFloat3x4(&m_instanceTransforms[j], m);
            ++j;
        }

        assert(j == m_instanceCount);

        const size_t instBytes = j * sizeof(XMFLOAT3X4);

        GraphicsResource inst = m_graphicsMemory->Allocate(instBytes);
        memcpy(inst.Memory(), m_instanceTransforms.get(), instBytes);

        D3D12_VERTEX_BUFFER_VIEW vertexBufferInst = {};
        vertexBufferInst.BufferLocation = inst.GpuAddress();
        vertexBufferInst.SizeInBytes = static_cast<UINT>(instBytes);
        vertexBufferInst.StrideInBytes = sizeof(XMFLOAT3X4);
        commandList->IASetVertexBuffers(1, 1, &vertexBufferInst);

        m_instancedEffect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo), m_states->AnisotropicWrap());
        m_instancedEffect->SetNormalTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::NormalMap));
        m_instancedEffect->SetWorld(XMMatrixTranslation(0.f, rowtop, 0.f));

        m_instancedEffect->Apply(commandList);
        m_teapot->DrawInstanced(commandList, m_instanceCount);
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

#ifdef REVERSEZ
    constexpr float c_zclear = 0.f;
#else
    constexpr float c_zclear = 1.f;
#endif

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, c_clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, c_zclear, 0, 0, nullptr);

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
    width = 1600;
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

    // Create effects.
    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

#ifdef REVERSEZ
    const auto& c_depthState = CommonStates::DepthReverseZ;
#else
    const auto& c_depthState = CommonStates::DepthDefault;
#endif

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pd);
        m_effect->EnableDefaultLighting();
    }

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            c_depthState,
            CommonStates::Wireframe,
            rtState);

        m_effectWireframe = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting, pd);
        m_effectWireframe->EnableDefaultLighting();
    }

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effectTexture = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectTexture->EnableDefaultLighting();

    }

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::AlphaBlend,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effectAlpha = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting, pd);
        m_effectAlpha->EnableDefaultLighting();
    }

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::AlphaBlend,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effectPMAlphaTexture = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectPMAlphaTexture->EnableDefaultLighting();
    }

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::NonPremultiplied,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effectAlphaTexture = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectAlphaTexture->EnableDefaultLighting();
    }

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effectLights = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectLights->EnableDefaultLighting();
    }

    {
        const EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effectFog = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::Fog | EffectFlags::PerPixelLighting, pd);
        m_effectFog->EnableDefaultLighting();

#ifdef LH_COORDS
        m_effectFog->SetFogStart(-6);
        m_effectFog->SetFogEnd(-8);
#else
        m_effectFog->SetFogStart(6);
        m_effectFog->SetFogEnd(8);
#endif
        m_effectFog->SetFogColor(c_clearColor);
    }

    {
        static const D3D12_INPUT_ELEMENT_DESC s_InputElements[] =
        {
            // GeometricPrimitive::VertexType
            { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
            { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
            { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
            // XMFLOAT3X4
            { "InstMatrix",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "InstMatrix",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "InstMatrix",  2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        };

        static const D3D12_INPUT_LAYOUT_DESC s_layout = { s_InputElements, static_cast<UINT>(std::size(s_InputElements)) };

        const EffectPipelineStateDescription pd(
            &s_layout,
            CommonStates::Opaque,
            c_depthState,
            CommonStates::CullCounterClockwise,
            rtState);

        m_instancedEffect = std::make_unique<NormalMapEffect>(device, EffectFlags::Fog | EffectFlags::Instancing, pd);
        m_instancedEffect->EnableDefaultLighting();

#ifdef LH_COORDS
        m_instancedEffect->SetFogStart(-9);
        m_instancedEffect->SetFogEnd(-10);
#else
        m_instancedEffect->SetFogStart(9);
        m_instancedEffect->SetFogEnd(10);
#endif

        m_instancedEffect->SetFogColor(c_clearColor);

        // Create instance transforms.
        size_t j = 0;
        for (float x = -8.f; x <= 8.f; x += 3.f)
        {
            ++j;
        }
        m_instanceCount = static_cast<UINT>(j);

        m_instanceTransforms = std::make_unique<XMFLOAT3X4[]>(j);

        constexpr XMFLOAT3X4 s_identity = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f };

        j = 0;
        for (float x = -8.f; x <= 8.f; x += 3.f)
        {
            m_instanceTransforms[j] = s_identity;
            m_instanceTransforms[j]._14 = x;
            ++j;
        }
    }

    // Create shapes.
#ifdef LH_COORDS
    bool rhcoords = false;
#else
    bool rhcoords = true;
#endif

    m_cube = GeometricPrimitive::CreateCube(1.f, rhcoords);
    m_box = GeometricPrimitive::CreateBox(XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f), rhcoords);
    m_sphere = GeometricPrimitive::CreateSphere(1.f, 16, rhcoords);
    m_geosphere = GeometricPrimitive::CreateGeoSphere(1.f, 3, rhcoords);
    m_cylinder = GeometricPrimitive::CreateCylinder(1.f, 1.f, 32, rhcoords);
    m_cone = GeometricPrimitive::CreateCone(1.f, 1.f, 32, rhcoords);
    m_torus = GeometricPrimitive::CreateTorus(1.f, 0.333f, 32, rhcoords);
    m_teapot = GeometricPrimitive::CreateTeapot(1.f, 8, rhcoords);
    m_tetra = GeometricPrimitive::CreateTetrahedron(0.75f, rhcoords);
    m_octa = GeometricPrimitive::CreateOctahedron(0.75f, rhcoords);
    m_dodec = GeometricPrimitive::CreateDodecahedron(0.5f, rhcoords);
    m_iso = GeometricPrimitive::CreateIcosahedron(0.5f, rhcoords);

    {
        GeometricPrimitive::VertexCollection customVerts;
        GeometricPrimitive::IndexCollection customIndices;
        GeometricPrimitive::CreateBox(customVerts, customIndices, XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f), rhcoords);

        assert(customVerts.size() == 24);
        assert(customIndices.size() == 36);

        for (auto& it : customVerts)
        {
            it.textureCoordinate.x *= 5.f;
            it.textureCoordinate.y *= 5.f;
        }

        m_customBox = GeometricPrimitive::CreateCustom(customVerts, customIndices);
    }

    {
        // Ensure VertexType alias is consistent with alternative client usage
        GeometricPrimitive::VertexCollection customVerts;
        GeometricPrimitive::IndexCollection customIndices;
        GeometricPrimitive::CreateBox(customVerts, customIndices, XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f), rhcoords);

        assert(customVerts.size() == 24);
        assert(customIndices.size() == 36);

        for (auto& it : customVerts)
        {
            it.textureCoordinate.x *= 5.f;
            it.textureCoordinate.y *= 5.f;
        }

        m_customBox2 = GeometricPrimitive::CreateCustom(customVerts, customIndices);
    }

    // Load textures.
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

    {
        ResourceUploadBatch resourceUpload(device);

        resourceUpload.Begin();

        // Convert some primitives to using static VB/IBs
        m_geosphere->LoadStaticBuffers(device, resourceUpload);
        m_cylinder->LoadStaticBuffers(device, resourceUpload);
        m_cone->LoadStaticBuffers(device, resourceUpload);

#ifndef USE_COPY_QUEUE
        m_torus->LoadStaticBuffers(device, resourceUpload);
#endif

#ifndef USE_COMPUTE_QUEUE
        m_teapot->LoadStaticBuffers(device, resourceUpload);
#endif

#ifdef GAMMA_CORRECT_RENDERING
        constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_FORCE_SRGB;
#else
        constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_DEFAULT;
#endif

        DX::ThrowIfFailed(
            CreateDDSTextureFromFileEx(device, resourceUpload, L"cat.dds",
                0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
                m_cat.ReleaseAndGetAddressOf()));

        CreateShaderResourceView(device, m_cat.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cat));

        DX::ThrowIfFailed(
            CreateDDSTextureFromFileEx(device, resourceUpload, L"dx5_logo.dds",
                0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
                m_dxLogo.ReleaseAndGetAddressOf()));

        CreateShaderResourceView(device, m_dxLogo.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo));

        DX::ThrowIfFailed(
            CreateDDSTextureFromFileEx(device, resourceUpload, L"reftexture.dds",
                0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
                m_refTexture.ReleaseAndGetAddressOf()));

        CreateShaderResourceView(device, m_refTexture.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::RefTexture));

        DX::ThrowIfFailed(
            CreateDDSTextureFromFileEx(device, resourceUpload, L"normalMap.dds",
                0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_DEFAULT,
                m_normalMap.ReleaseAndGetAddressOf()));

        CreateShaderResourceView(device, m_normalMap.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::NormalMap));

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

        uploadResourcesFinished.wait();
    }

    // Copy Queue test
#ifdef USE_COPY_QUEUE
    {
        ResourceUploadBatch resourceUpload(device);

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

        resourceUpload.Begin(queueDesc.Type);

        DX::ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_GRAPHICS_PPV_ARGS(m_copyQueue.ReleaseAndGetAddressOf())));

        m_copyQueue->SetName(L"CopyTest");

        m_torus->LoadStaticBuffers(device, resourceUpload);

        auto uploadResourcesFinished = resourceUpload.End(m_copyQueue.Get());
        uploadResourcesFinished.wait();

        m_firstFrame = true;
    }
#endif // USE_COPY_QUEUE

    // Compute Queue test
#ifdef USE_COMPUTE_QUEUE
    {
        ResourceUploadBatch resourceUpload(device);

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

        resourceUpload.Begin(queueDesc.Type);

        DX::ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_GRAPHICS_PPV_ARGS(m_computeQueue.ReleaseAndGetAddressOf())));

        m_computeQueue->SetName(L"ComputeTest");

        m_teapot->LoadStaticBuffers(device, resourceUpload);

        auto uploadResourcesFinished = resourceUpload.End(m_computeQueue.Get());
        uploadResourcesFinished.wait();

        m_firstFrame = true;
    }
#endif // USE_COMPUTE_QUEUE

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 9.f, 0.f } } };

    auto const size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef REVERSEZ
    constexpr float c_nearz = 10.f;
    constexpr float c_farz = 1.f;
#else
    constexpr float c_nearz = 1.f;
    constexpr float c_farz = 10.f;
#endif

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovLH(1, aspect, c_nearz, c_farz);
#else
    XMMATRIX view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovRH(1, aspect, c_nearz, c_farz);
#endif

#ifdef UWP
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        projection *= orient;
    }
#endif

    m_effect->SetView(view);
    m_effectWireframe->SetView(view);
    m_effectTexture->SetView(view);
    m_effectAlpha->SetView(view);
    m_effectPMAlphaTexture->SetView(view);
    m_effectAlphaTexture->SetView(view);
    m_effectLights->SetView(view);
    m_effectFog->SetView(view);
    m_instancedEffect->SetView(view);
    
    m_effect->SetProjection(projection);
    m_effectWireframe->SetProjection(projection);
    m_effectTexture->SetProjection(projection);
    m_effectAlpha->SetProjection(projection);
    m_effectPMAlphaTexture->SetProjection(projection);
    m_effectAlphaTexture->SetProjection(projection);
    m_effectLights->SetProjection(projection);
    m_effectFog->SetProjection(projection);
    m_instancedEffect->SetProjection(projection);
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_cube.reset();
    m_box.reset();
    m_sphere.reset();
    m_geosphere.reset();
    m_cylinder.reset();
    m_cone.reset();
    m_torus.reset();
    m_teapot.reset();
    m_tetra.reset();
    m_octa.reset();
    m_dodec.reset();
    m_iso.reset();
    m_customBox.reset();
    m_customBox2.reset();

    m_effect.reset();
    m_effectWireframe.reset();
    m_effectTexture.reset();
    m_effectAlpha.reset();
    m_effectPMAlphaTexture.reset();
    m_effectAlphaTexture.reset();
    m_effectLights.reset();
    m_effectFog.reset();
    m_instancedEffect.reset();

    m_cat.Reset();
    m_dxLogo.Reset();
    m_refTexture.Reset();
    m_normalMap.Reset();

    m_resourceDescriptors.reset();
    m_states.reset();
    m_graphicsMemory.reset();

    m_copyQueue.Reset();
    m_computeQueue.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion
