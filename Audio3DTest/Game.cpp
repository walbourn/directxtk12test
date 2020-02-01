//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK for Audio - Positional
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

// Build for LH vs. RH coords
//#define LH_COORDS

// Build with master limiter set or not
#define USE_MASTER_LIMITER

// Build with Reverb enabled or not
#define USE_REVERB

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const X3DAUDIO_CONE Listener_DirectionalCone = { X3DAUDIO_PI * 5.0f / 6.0f, X3DAUDIO_PI * 11.0f / 6.0f, 1.0f, 0.75f, 0.0f, 0.25f, 0.708f, 1.0f };

    const X3DAUDIO_CONE Emitter_DirectionalCone = { 0.f, 0.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f };

    const X3DAUDIO_DISTANCE_CURVE_POINT Emitter_LFE_CurvePoints[3] = { 0.0f, 1.0f, 0.25f, 0.0f, 1.0f, 0.0f };
    const X3DAUDIO_DISTANCE_CURVE       Emitter_LFE_Curve = { (X3DAUDIO_DISTANCE_CURVE_POINT*)&Emitter_LFE_CurvePoints[0], 3 };

    const X3DAUDIO_DISTANCE_CURVE_POINT Emitter_Reverb_CurvePoints[3] = { 0.0f, 0.5f, 0.75f, 1.0f, 1.0f, 0.0f };
    const X3DAUDIO_DISTANCE_CURVE       Emitter_Reverb_Curve = { (X3DAUDIO_DISTANCE_CURVE_POINT*)&Emitter_Reverb_CurvePoints[0], 3 };

    void SetDeviceString(_In_ AudioEngine* engine, _Out_writes_(maxsize) wchar_t* deviceStr, size_t maxsize)
    {
        if (engine->IsAudioDevicePresent())
        {
            auto wfx = engine->GetOutputFormat();

            const char* speakerConfig;
            switch (wfx.dwChannelMask)
            {
            case SPEAKER_MONO:              speakerConfig = "Mono"; break;
            case SPEAKER_STEREO:            speakerConfig = "Stereo"; break;
            case SPEAKER_2POINT1:           speakerConfig = "2.1"; break;
            case SPEAKER_SURROUND:          speakerConfig = "Surround"; break;
            case SPEAKER_QUAD:              speakerConfig = "Quad"; break;
            case SPEAKER_4POINT1:           speakerConfig = "4.1"; break;
            case SPEAKER_5POINT1:           speakerConfig = "5.1"; break;
            case SPEAKER_7POINT1:           speakerConfig = "7.1"; break;
            case SPEAKER_5POINT1_SURROUND:  speakerConfig = "Surround5.1"; break;
            case SPEAKER_7POINT1_SURROUND:  speakerConfig = "Surround7.1"; break;
            default:                        speakerConfig = "(unknown)"; break;
            }

            swprintf_s(deviceStr, maxsize, L"Output format rate %u, channels %u, %hs (%08X)", wfx.Format.nSamplesPerSec, wfx.Format.nChannels, speakerConfig, wfx.dwChannelMask);
        }
        else
        {
            wcscpy_s(deviceStr, maxsize, L"No default audio device found, running in 'silent mode'");
        }
    }
}

Game::Game() noexcept(false) :
    m_critError(false),
    m_retrydefault(false),
    m_newAudio(false),
    m_deviceStr{}
{
#ifdef GAMMA_CORRECT_RENDERING
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>();
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_deviceResources->RegisterDeviceNotify(this);
#endif

    // Set up 3D positional audio structures
    m_listener.pCone = const_cast<X3DAUDIO_CONE*>(&Listener_DirectionalCone);

    m_emitter.SetPosition(XMFLOAT3(10.f, 0.f, 0.f));
    m_emitter.pLFECurve = const_cast<X3DAUDIO_DISTANCE_CURVE*>(&Emitter_LFE_Curve);
    m_emitter.pReverbCurve = const_cast<X3DAUDIO_DISTANCE_CURVE*>(&Emitter_Reverb_Curve);
    m_emitter.CurveDistanceScaler = 14.f;
    m_emitter.pCone = const_cast<X3DAUDIO_CONE*>(&Emitter_DirectionalCone);
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }

    if (m_audEngine)
    {
        m_audEngine->Suspend();
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

    // Initialize audio
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;

#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif

#ifdef USE_REVERB
    eflags = eflags | AudioEngine_EnvironmentalReverb | AudioEngine_ReverbUseFilters;
#endif

#ifdef USE_MASTER_LIMITER
    eflags = eflags | AudioEngine_UseMasteringLimiter;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audEngine->SetReverb(Reverb_Default);

    SetDeviceString(m_audEngine.get(), m_deviceStr, 256);

    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"heli.wav");

    m_effect = m_soundEffect->CreateInstance(SoundEffectInstance_Use3D | SoundEffectInstance_ReverbUseFilters);
    if (!m_effect)
    {
        throw std::exception("heli.wav");
    }

    m_effect->Play(true);
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    AudioRender();

    Render();
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

    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitGame();
    }

#ifdef USE_REVERB
    if (m_keyboardButtons.IsKeyPressed(Keyboard::D1))
    {
        m_audEngine->SetReverb(Reverb_Default);
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::D2))
    {
        m_audEngine->SetReverb(Reverb_Bathroom);
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::D3))
    {
        m_audEngine->SetReverb(Reverb_Cave);
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::D0))
    {
        m_audEngine->SetReverb(Reverb_Off);
    }
#endif

    // Time-based animation
    float time = static_cast<float>(timer.GetTotalSeconds());

    float posx = cosf(time) * 10.f;
    float posz = sinf(time) * 5.f;
#ifdef LH_COORDS
    m_emitterMatrix = XMMatrixTranslation(posx, 0, -posz);
#else
    m_emitterMatrix = XMMatrixTranslation(posx, 0, posz);
#endif

    float dt = static_cast<float>(m_timer.GetElapsedSeconds());

    m_emitter.Update(m_emitterMatrix.Translation(), g_XMIdentityR1, dt);

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

    XMVECTORF32 red, yellow;
#ifdef GAMMA_CORRECT_RENDERING
    red.v = XMColorSRGBToRGB(Colors::Red);
    yellow.v = XMColorSRGBToRGB(Colors::Yellow);
#else
    red.v = Colors::Red;
    yellow.v = Colors::Yellow;
#endif

    auto heap = m_resourceDescriptors->Heap();
    commandList->SetDescriptorHeaps(1, &heap);

    m_sphereEffect->SetMatrices(m_listenerMatrix, m_view, m_projection);
    m_sphereEffect->SetDiffuseColor(yellow);
    m_sphereEffect->Apply(commandList);
    m_sphere->Draw(commandList);

    m_sphereEffect->SetWorld(m_emitterMatrix);
    m_sphereEffect->SetDiffuseColor(red);
    m_sphereEffect->Apply(commandList);
    m_sphere->Draw(commandList);

    auto stats = m_audEngine->GetStatistics();

    wchar_t statsStr[256] = {};
    swprintf_s(statsStr, L"Playing: %zu / %zu; Instances %zu; Voices %zu / %zu / %zu / %zu; %zu audio bytes",
        stats.playingOneShots, stats.playingInstances,
        stats.allocatedInstances, stats.allocatedVoices, stats.allocatedVoices3d,
        stats.allocatedVoicesOneShot, stats.allocatedVoicesIdle,
        stats.audioBytes);

    m_spriteBatch->Begin(commandList);

    m_comicFont->DrawString(m_spriteBatch.get(), m_deviceStr, XMFLOAT2(0, 0));
    m_comicFont->DrawString(m_spriteBatch.get(), statsStr, XMFLOAT2(0, 36));

    if (m_critError)
    {
        m_comicFont->DrawString(m_spriteBatch.get(), L"ERROR: Critical Error detected", XMFLOAT2(0, 72), Colors::Red);
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

void Game::AudioRender()
{
    if (m_retrydefault)
    {
        // We try to reset the audio using the default device
        m_retrydefault = false;

        if (m_audEngine->Reset())
        {
            // It worked, so we reset our sound
            m_critError = false;
            SetDeviceString(m_audEngine.get(), m_deviceStr, 256);
            m_effect->Play(true);
        }
    }

#ifdef LH_COORDS
    m_effect->Apply3D(m_listener, m_emitter, false);
#else
    m_effect->Apply3D(m_listener, m_emitter);
#endif

    if (!m_audEngine->Update())
    {
        // We are running in silent mode
        if (m_audEngine->IsCriticalError())
        {
            if (!m_critError)
            {
                // First will retry using the default audio device
                m_retrydefault = true;
                m_critError = true;
            }
        }

        if (m_newAudio)
        {
            // There's new audio and we are in silent mode, so retry default
            m_retrydefault = true;
        }
    }
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

    m_audEngine->Suspend();
}

void Game::OnResuming()
{
#if defined(_XBOX_ONE) && defined(_TITLE)
    auto queue = m_deviceResources->GetCommandQueue();
    queue->ResumeX();
#endif

    m_timer.ResetElapsedTime();
    m_keyboardButtons.Reset();
    m_audEngine->Resume();
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

void Game::OnAudioDeviceChange()
{
    m_newAudio = true;
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
        1);

#ifdef LH_COORDS
    m_sphere = GeometricPrimitive::CreateSphere(1.f, 16, false);
#else
    m_sphere = GeometricPrimitive::CreateSphere(1.f, 16);
#endif

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        m_sphereEffect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pd);
        m_sphereEffect->EnableDefaultLighting();
    }

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    {
        SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    m_comicFont = std::make_unique<SpriteFont>(device, resourceUpload,
        L"comic.spritefont",
        m_resourceDescriptors->GetFirstCpuHandle(),
        m_resourceDescriptors->GetFirstGpuHandle());

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { 0, 14, 0 };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    m_listenerMatrix = XMMatrixTranslation(0, 0, -7.f);
    m_view = XMMatrixLookAtLH(cameraPosition, m_listenerMatrix.Translation(), XMVectorSet(0, 0, 1, 0));
    m_projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 100);
#else
    m_listenerMatrix = XMMatrixTranslation(0, 0, 7.f);
    m_view = XMMatrixLookAtRH(cameraPosition, m_listenerMatrix.Translation(), XMVectorSet(0, 0, -1, 0));
    m_projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 100);
#endif

    m_listener.SetPosition(m_listenerMatrix.Translation());

#ifdef LH_COORDS
    m_listener.SetOrientation(XMFLOAT3(0.f, 0.f, 1.f), XMFLOAT3(0.f, 1.f, 0.f));
#else
    m_listener.SetOrientation(XMFLOAT3(0.f, 0.f, -1.f), XMFLOAT3(0.f, 1.f, 0.f));
#endif

    auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    auto rotation = m_deviceResources->GetRotation();
    m_spriteBatch->SetRotation(rotation);

    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        m_projection *= orient;
    }
#endif
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_sphereEffect.reset();
    m_sphere.reset();
    m_spriteBatch.reset();
    m_comicFont.reset();
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
