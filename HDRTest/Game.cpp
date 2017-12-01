//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK PostProcess (HDR10/ToneMap)
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

// Build for LH vs. RH coords
//#define LH_COORDS

// For UWP/PC, this tests using a linear F16 swapchain intead of HDR10
//#define TEST_HDR_LINEAR

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const XMVECTORF32 c_BrightYellow = { 2.f, 2.f, 0.f, 1.f };

    const XMVECTORF32 c_DimWhite = { .5f, .5f, .5f, 1.f };
    const XMVECTORF32 c_BrightWhite = { 2.f, 2.f, 2.f, 1.f };
    const XMVECTORF32 c_VeryBrightWhite = { 4.f, 4.f, 4.f, 1.f };

    const float row0 = -2.f;

    const float col0 = -5.f;
    const float col1 = -3.5f;
    const float col2 = -1.f;
    const float col3 = 1.f;
    const float col4 = 3.5f;
    const float col5 = 5.f;
}

#if defined(_XBOX_ONE) && defined(_TITLE)
extern bool g_HDRMode;
#endif

Game::Game()
{
#if defined(_XBOX_ONE) && defined(_TITLE)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD
        | DX::DeviceResources::c_EnableHDR);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(
#if defined(TEST_HDR_LINEAR)
        DXGI_FORMAT_R16G16B16A16_FLOAT,
#else
        DXGI_FORMAT_R10G10B10A2_UNORM,
#endif
        DXGI_FORMAT_D32_FLOAT, 2,
        D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_EnableHDR);
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_deviceResources->RegisterDeviceNotify(this);
#endif

    // Set up for HDR rendering.
    m_hdrScene = std::make_unique<DX::RenderTexture>(DXGI_FORMAT_R16G16B16A16_FLOAT);

    XMVECTORF32 color;
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
    m_hdrScene->SetClearColor(color);
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

    auto commandList = m_deviceResources->GetCommandList();
    m_hdrScene->BeginScene(commandList);

    Clear();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    auto vp = m_deviceResources->GetOutputSize();
    auto safeRect = Viewport::ComputeTitleSafeArea(vp.right - vp.left, vp.bottom - vp.top);

    long w = safeRect.right - safeRect.left;
    long h = safeRect.bottom - safeRect.top;

    m_batch->Begin(commandList);

    auto hdrImage1 = m_resourceDescriptors->GetGpuHandle(Descriptors::HDRImage1);
    auto hdrImageSize1 = GetTextureSize(m_hdrImage1.Get());

    auto hdrImage2 = m_resourceDescriptors->GetGpuHandle(Descriptors::HDRImage2);
    auto hdrImageSize2 = GetTextureSize(m_hdrImage2.Get());

    RECT r = { safeRect.left, safeRect.top,
        safeRect.left + (w / 2),
        safeRect.top + (h / 2) };
    m_batch->Draw(hdrImage1, hdrImageSize1, r);

    r = { safeRect.left + (w / 2), safeRect.top,
        safeRect.left + (w / 2) + (w / 4),
        safeRect.top + (h / 4) };
    m_batch->Draw(hdrImage2, hdrImageSize2, r, c_DimWhite);

    r = { safeRect.left + (w / 2) + (w / 4), safeRect.top,
        safeRect.left + (w / 2) + (w / 4) * 2,
        safeRect.top + (h / 4) };
    m_batch->Draw(hdrImage2, hdrImageSize2, r);

    r = { safeRect.left + (w / 2), safeRect.top + (h / 4),
        safeRect.left + (w / 2) + (w / 4),
        safeRect.top + (h / 4) * 2 };
    m_batch->Draw(hdrImage2, hdrImageSize2, r, c_BrightWhite);

    r = { safeRect.left + (w / 2) + (w / 4), safeRect.top + (h / 4),
        safeRect.left + (w / 2) + (w / 4) * 2,
        safeRect.top + (h / 4) * 2 };
    m_batch->Draw(hdrImage2, hdrImageSize2, r, c_VeryBrightWhite);

    m_batch->End();

    // Time-based animation
    float time = static_cast<float>(m_timer.GetTotalSeconds());

    float alphaFade = (sin(time * 2) + 1) / 2;

    if (alphaFade >= 1)
        alphaFade = 1 - FLT_EPSILON;

    float yaw = time * 0.4f;
    float pitch = time * 0.7f;
    float roll = time * 1.1f;

    XMMATRIX world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    XMVECTOR quat = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);

    m_flatEffect->SetMatrices(world* XMMatrixTranslation(col0, row0, 0), m_view, m_projection);
    m_flatEffect->SetTexture(hdrImage1, m_states->LinearWrap());
    m_flatEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_flatEffect->SetMatrices(world* XMMatrixTranslation(col1, row0, 0), m_view, m_projection);
    m_flatEffect->SetTexture(hdrImage2, m_states->LinearWrap());
    m_flatEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_basicEffect->SetMatrices(world* XMMatrixTranslation(col2, row0, 0), m_view, m_projection);
    m_basicEffect->SetTexture(hdrImage1, m_states->LinearWrap());
    m_basicEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_basicEffect->SetMatrices(world* XMMatrixTranslation(col3, row0, 0), m_view, m_projection);
    m_basicEffect->SetTexture(hdrImage2, m_states->LinearWrap());
    m_basicEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_brightEffect->SetMatrices(world* XMMatrixTranslation(col4, row0, 0), m_view, m_projection);
    m_brightEffect->SetTexture(hdrImage1, m_states->LinearWrap());
    m_brightEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_brightEffect->SetMatrices(world* XMMatrixTranslation(col5, row0, 0), m_view, m_projection);
    m_brightEffect->SetTexture(hdrImage2, m_states->LinearWrap());
    m_brightEffect->Apply(commandList);
    m_shape->Draw(commandList);

    // Render HUD
    m_batch->Begin(commandList);

    const wchar_t* info = nullptr;

#if defined(_XBOX_ONE) && defined(_TITLE)
    info = (g_HDRMode) ? L"TV in HDR Mode" : L"TV in SDR Mode";
#else
    switch (m_deviceResources->GetColorSpace())
    {
    default:
        info = L"SRGB";
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        info = L"HDR10";
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        info = L"Linear";
        break;
    }
#endif

    m_font->DrawString(m_batch.get(), info, XMFLOAT2(float(safeRect.right - (safeRect.right / 4)), float(safeRect.bottom - (h / 16))), c_BrightYellow);

    m_batch->End();

    PIXEndEvent(commandList);

    // Tonemap the frame.
    m_hdrScene->EndScene(commandList);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Tonemap");

#if defined(_XBOX_ONE) && defined(_TITLE)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptors[2] = { m_deviceResources->GetRenderTargetView(), m_deviceResources->GetGameDVRRenderTargetView() };
    commandList->OMSetRenderTargets(2, rtvDescriptors, FALSE, nullptr);

    m_toneMap->Process(commandList);
#else
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

    switch (m_deviceResources->GetColorSpace())
    {
    default:
        m_toneMap->Process(commandList);
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        m_toneMapHDR10->Process(commandList);
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        m_toneMapLinear->Process(commandList);
        break;
    }
#endif

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
    auto rtvDescriptor = m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene);
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    XMVECTORF32 color;
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
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
}

void Game::OnResuming()
{
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

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_hdrScene->SetDevice(device,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene));

    RenderTargetState hdrState(m_hdrScene->GetFormat(), m_deviceResources->GetDepthBufferFormat());

#ifdef LH_COORDS
    m_shape = GeometricPrimitive::CreateCube(1.f, false);
#else
    m_shape = GeometricPrimitive::CreateCube();
#endif

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            hdrState);

        m_flatEffect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pd);
        m_flatEffect->SetAmbientLightColor(Colors::White);
        m_flatEffect->DisableSpecular();

        m_basicEffect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pd);
        m_basicEffect->EnableDefaultLighting();

        m_brightEffect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pd);
        m_brightEffect->EnableDefaultLighting();
        m_brightEffect->SetLightDiffuseColor(0, Colors::White);
        m_brightEffect->SetLightDiffuseColor(1, c_VeryBrightWhite);
        m_brightEffect->SetLightDiffuseColor(2, Colors::White);
    }
       
    m_states = std::make_unique<CommonStates>(device);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    {
        SpriteBatchPipelineStateDescription pd(hdrState);
        m_batch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    m_font = std::make_unique<SpriteFont>(device, resourceUpload, L"comic.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::Font),
        m_resourceDescriptors->GetGpuHandle(Descriptors::Font));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"HDR_029_Sky_Cloudy_Ref.dds",
            m_hdrImage1.ReleaseAndGetAddressOf())
    );

    CreateShaderResourceView(device, m_hdrImage1.Get(),
        m_resourceDescriptors->GetCpuHandle(Descriptors::HDRImage1));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"HDR_112_River_Road_2_Ref.dds",
            m_hdrImage2.ReleaseAndGetAddressOf())
    );

    CreateShaderResourceView(device, m_hdrImage2.Get(),
        m_resourceDescriptors->GetCpuHandle(Descriptors::HDRImage2));

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
#if defined(_XBOX_ONE) && defined(_TITLE)
    rtState.numRenderTargets = 2;
    rtState.rtvFormats[1] = m_deviceResources->GetGameDVRFormat();
#endif

    m_toneMap = std::make_unique<ToneMapPostProcess>(
        device,
        rtState,
        ToneMapPostProcess::ACESFilmic, (m_deviceResources->GetBackBufferFormat() == DXGI_FORMAT_R16G16B16A16_FLOAT) ? ToneMapPostProcess::Linear : ToneMapPostProcess::SRGB
#if defined(_XBOX_ONE) && defined(_TITLE)
        , true
#endif
        );

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::Linear);
    m_toneMapHDR10 = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::ST2084);
#endif

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { 0, 0, 7 };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    m_view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 10);
#else
    m_view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 10);
#endif

    // Set windows size for HDR.
    m_hdrScene->SetWindow(size);

    m_toneMap->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
    m_toneMapHDR10->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_batch->SetRotation(m_deviceResources->GetRotation());
#endif

    m_batch->SetViewport(m_deviceResources->GetScreenViewport());
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_batch.reset();
    m_font.reset();

    m_shape.reset();
    m_states.reset();

    m_flatEffect.reset();
    m_basicEffect.reset();
    m_brightEffect.reset();

    m_hdrImage1.Reset();
    m_hdrImage2.Reset();

    m_toneMap.reset();

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear.reset();
    m_toneMapHDR10.reset();
#endif

    m_hdrScene->ReleaseDevice();

    m_resourceDescriptors.reset();
    m_renderDescriptors.reset();

    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion
