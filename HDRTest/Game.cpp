//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK PostProcess (HDR10/ToneMap)
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

// Build for LH vs. RH coords
//#define LH_COORDS

// For UWP/PC, this tests using a linear F16 swapchain intead of HDR10
//#define TEST_HDR_LINEAR

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

namespace
{
    constexpr XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };

    constexpr XMVECTORF32 c_BrightYellow = { { { 2.f, 2.f, 0.f, 1.f } } };

    constexpr XMVECTORF32 c_DimWhite = { { { .5f, .5f, .5f, 1.f } } };
    constexpr XMVECTORF32 c_BrightWhite = { { { 2.f, 2.f, 2.f, 1.f } } };
    constexpr XMVECTORF32 c_VeryBrightWhite = { { { 4.f, 4.f, 4.f, 1.f } } };

    constexpr float row0 = -2.f;

    constexpr float col0 = -5.f;
    constexpr float col1 = -3.5f;
    constexpr float col2 = -1.f;
    constexpr float col3 = 1.f;
    constexpr float col4 = 3.5f;
    constexpr float col5 = 5.f;

    const XMMATRIX c_fromExpanded709to2020 = // Custom Rec.709 into Rec.2020
    {
          0.6274040f, 0.0457456f, -0.00121055f, 0.f,
          0.3292820f,  0.941777f,   0.0176041f, 0.f,
          0.0433136f, 0.0124772f,    0.983607f, 0.f,
                 0.f,        0.f,          0.f, 1.f
    };
}

#ifdef XBOX
extern bool g_HDRMode;
#endif

Game::Game() noexcept(false) :
    m_toneMapMode(ToneMapPostProcess::Reinhard),
    m_hdr10Rotation(ToneMapPostProcess::HDTV_to_UHDTV),
    m_frame(0)
{
#if defined(TEST_HDR_LINEAR) && !defined(XBOX)
    constexpr DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
#else
    constexpr DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
#endif

    unsigned int deviceOptions = DX::DeviceResources::c_EnableHDR;

#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2,
        deviceOptions | DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        deviceOptions | DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        deviceOptions);
#endif

#ifdef _GAMING_XBOX
    m_deviceResources->SetClearColor(c_clearColor);
#endif

#ifdef LOSTDEVICE
    m_deviceResources->RegisterDeviceNotify(this);
#endif

    // Set up for HDR rendering.
    m_hdrScene = std::make_unique<DX::RenderTexture>(DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_hdrScene->SetClearColor(c_clearColor);
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
void Game::Update(DX::StepTimer const&)
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

        if (m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED)
        {
            CycleToneMapOperator();
        }

        if (m_gamePadButtons.x == GamePad::ButtonStateTracker::PRESSED)
        {
            CycleColorRotation();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    m_keyboardButtons.Update(kb);

    if (m_keyboardButtons.pressed.T || m_keyboardButtons.pressed.Space)
    {
        CycleToneMapOperator();
    }

    if (m_keyboardButtons.pressed.C || m_keyboardButtons.pressed.Enter)
    {
        CycleColorRotation();
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
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    auto vp = m_deviceResources->GetOutputSize();
    auto safeRect = Viewport::ComputeTitleSafeArea(UINT(vp.right - vp.left), UINT(vp.bottom - vp.top));

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

    m_flatEffect->SetMatrices(world * XMMatrixTranslation(col0, row0, 0), m_view, m_projection);
    m_flatEffect->SetTexture(hdrImage1, m_states->LinearWrap());
    m_flatEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_flatEffect->SetMatrices(world * XMMatrixTranslation(col1, row0, 0), m_view, m_projection);
    m_flatEffect->SetTexture(hdrImage2, m_states->LinearWrap());
    m_flatEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_basicEffect->SetMatrices(world * XMMatrixTranslation(col2, row0, 0), m_view, m_projection);
    m_basicEffect->SetTexture(hdrImage1, m_states->LinearWrap());
    m_basicEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_basicEffect->SetMatrices(world * XMMatrixTranslation(col3, row0, 0), m_view, m_projection);
    m_basicEffect->SetTexture(hdrImage2, m_states->LinearWrap());
    m_basicEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_brightEffect->SetMatrices(world * XMMatrixTranslation(col4, row0, 0), m_view, m_projection);
    m_brightEffect->SetTexture(hdrImage1, m_states->LinearWrap());
    m_brightEffect->Apply(commandList);
    m_shape->Draw(commandList);

    m_brightEffect->SetMatrices(world * XMMatrixTranslation(col5, row0, 0), m_view, m_projection);
    m_brightEffect->SetTexture(hdrImage2, m_states->LinearWrap());
    m_brightEffect->Apply(commandList);
    m_shape->Draw(commandList);

    // Render HUD
    m_batch->Begin(commandList);

#ifdef XBOX
    wchar_t info[128] = {};
    if (g_HDRMode)
    {
        const wchar_t* hdrRot = nullptr;
        switch (m_hdr10Rotation)
        {
        case ToneMapPostProcess::DCI_P3_D65_to_UHDTV:   hdrRot = L"P3-D65->UHDTV"; break;
        case ToneMapPostProcess::HDTV_to_DCI_P3_D65:    hdrRot = L"HDTV->P3-D65"; break;
        case 3:                                         hdrRot = L"Custom: X709"; break;
        default:                                        hdrRot = L"HDTV->UHDTV"; break;
        }

        const wchar_t* toneMapper = nullptr;
        switch (m_toneMapMode)
        {
        case ToneMapPostProcess::Reinhard: toneMapper = L"Reinhard"; break;
        case ToneMapPostProcess::ACESFilmic: toneMapper = L"ACES Filmic"; break;
        default: toneMapper = L"Saturate"; break;
        }

        swprintf_s(info, L"HDR10 (%ls) [GameDVR: %ls]", hdrRot, toneMapper);
    }
    else
    {
        switch (m_toneMapMode)
        {
        case ToneMapPostProcess::Reinhard: wcscpy_s(info, L"Reinhard"); break;
        case ToneMapPostProcess::ACESFilmic: wcscpy_s(info, L"ACES Filmic"); break;
        default: wcscpy_s(info, L"Saturate"); break;
        }
    }
#else // !XBOX
    const wchar_t* info = nullptr;
    switch (m_deviceResources->GetColorSpace())
    {
    default:
        switch (m_toneMapMode)
        {
        case ToneMapPostProcess::Reinhard:   info = L"Reinhard"; break;
        case ToneMapPostProcess::ACESFilmic: info = L"ACES Filmic"; break;
        default:                             info = L"Saturate"; break;
        }
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        switch (m_hdr10Rotation)
        {
        case ToneMapPostProcess::DCI_P3_D65_to_UHDTV:   info = L"HDR10 (P3-D65->UHDTV)"; break;
        case ToneMapPostProcess::HDTV_to_DCI_P3_D65:    info = L"HDR10 (HDTV->P3-D65)"; break;
        case 3:                                         info = L"HDR10 (Custom: X709)"; break;
        default:                                        info = L"HDR10 (HDTV->UHDTV)"; break;
        }
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

    ToneMapPostProcess* toneMap = nullptr;
#ifdef XBOX
    toneMap = m_toneMap[m_toneMapMode].get();
#else
    switch (m_deviceResources->GetColorSpace())
    {
    default:
        toneMap = m_toneMap[m_toneMapMode].get();
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        toneMap = m_toneMapHDR10.get();
        break;

    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        toneMap = m_toneMapLinear.get();
        break;
    }
#endif

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Tonemap");

    if (m_hdr10Rotation == 3)
    {
        toneMap->SetColorRotation(c_fromExpanded709to2020);
    }
    else
    {
        toneMap->SetColorRotation(static_cast<ToneMapPostProcess::ColorPrimaryRotation>(m_hdr10Rotation));
    }

#ifdef XBOX
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptors[2] = { m_deviceResources->GetRenderTargetView(), m_deviceResources->GetGameDVRRenderTargetView() };
    commandList->OMSetRenderTargets(2, rtvDescriptors, FALSE, nullptr);
#else
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
#endif

    toneMap->Process(commandList);

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
    const auto rtvDescriptor = m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene);
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
    m_keyboardButtons.Reset();
    m_gamePadButtons.Reset();
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

    m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_hdrScene->SetDevice(device,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene));

    const RenderTargetState hdrState(m_hdrScene->GetFormat(),
        m_deviceResources->GetDepthBufferFormat());

#ifdef LH_COORDS
    m_shape = GeometricPrimitive::CreateCube(1.f, false);
#else
    m_shape = GeometricPrimitive::CreateCube();
#endif

    {
        const EffectPipelineStateDescription pd(
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
        const SpriteBatchPipelineStateDescription pd(hdrState);
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
#ifdef XBOX
    rtState.numRenderTargets = 2;
    rtState.rtvFormats[1] = m_deviceResources->GetGameDVRFormat();
#endif

    for (unsigned int mode = ToneMapPostProcess::Saturate; mode < ToneMapPostProcess::Operator_Max; ++mode)
    {
        m_toneMap[mode] = std::make_unique<ToneMapPostProcess>(
            device,
            rtState,
            static_cast<ToneMapPostProcess::Operator>(mode),
            (m_deviceResources->GetBackBufferFormat() == DXGI_FORMAT_R16G16B16A16_FLOAT) ? ToneMapPostProcess::Linear : ToneMapPostProcess::SRGB
#ifdef XBOX
            , true
#endif
            );
    }

#ifndef XBOX
    m_toneMapLinear = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::Linear);
    m_toneMapHDR10 = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::ST2084);
#endif

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 7.f, 0.f } } };

    const auto size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    m_view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 10);
#else
    m_view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 10);
#endif

    // Set windows size for HDR.
    m_hdrScene->SetWindow(size);

    for (unsigned int mode = ToneMapPostProcess::Saturate; mode < ToneMapPostProcess::Operator_Max; ++mode)
    {
        m_toneMap[mode]->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
    }

#ifndef XBOX
    m_toneMapLinear->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
    m_toneMapHDR10->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
#endif

#ifdef UWP
    m_batch->SetRotation(m_deviceResources->GetRotation());
#endif

    m_batch->SetViewport(m_deviceResources->GetScreenViewport());
}

#ifdef LOSTDEVICE
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

    for (unsigned int mode = ToneMapPostProcess::Saturate; mode < ToneMapPostProcess::Operator_Max; ++mode)
    {
        m_toneMap[mode].reset();
    }

    m_toneMapLinear.reset();
    m_toneMapHDR10.reset();

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

void Game::CycleToneMapOperator()
{
#ifndef XBOX
    if (m_deviceResources->GetColorSpace() != DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
        return;
#endif

    m_toneMapMode += 1;

    if (m_toneMapMode >= static_cast<int>(ToneMapPostProcess::Operator_Max))
    {
        m_toneMapMode = ToneMapPostProcess::Saturate;
    }
}

void Game::CycleColorRotation()
{
#ifndef XBOX
    if (m_deviceResources->GetColorSpace() != DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
        return;
#endif

    m_hdr10Rotation += 1;

    if (m_hdr10Rotation > 3)
    {
        m_hdr10Rotation = ToneMapPostProcess::HDTV_to_UHDTV;
    }
}
