//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK for Audio - Positional
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include "FindMedia.h"

#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

// Build with master limiter set or not
#define USE_MASTER_LIMITER

// Build with Reverb enabled or not
#define USE_REVERB

// Enable zero-center
//#define USE_ZERO_CENTER

// Enable doppler effect
#define USE_DOPPLER

// Build with custom emitter curves
//#define USE_CUSTOM_CURVES

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

// AudioListener
static_assert(std::is_nothrow_default_constructible<AudioListener>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<AudioListener>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<AudioListener>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<AudioListener>::value, "Move Ctor.");
static_assert(std::is_move_assignable<AudioListener>::value, "Move Assign.");

// AudioEmitter
static_assert(std::is_nothrow_default_constructible<AudioEmitter>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<AudioEmitter>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<AudioEmitter>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<AudioEmitter>::value, "Move Ctor.");
static_assert(std::is_move_assignable<AudioEmitter>::value, "Move Assign.");

namespace
{
    constexpr X3DAUDIO_CONE Listener_DirectionalCone = { X3DAUDIO_PI * 5.0f / 6.0f, X3DAUDIO_PI * 11.0f / 6.0f, 1.0f, 0.75f, 0.0f, 0.25f, 0.708f, 1.0f };

    constexpr X3DAUDIO_CONE Emitter_DirectionalCone = { 0.f, 0.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f };

#ifdef USE_CUSTOM_CURVES
    constexpr X3DAUDIO_DISTANCE_CURVE_POINT Emitter_LFE_CurvePoints[3] = { { 0.0f, 1.0f }, { 0.25f, 0.0f }, { 1.0f, 0.0f } };
    constexpr X3DAUDIO_DISTANCE_CURVE       Emitter_LFE_Curve = { (X3DAUDIO_DISTANCE_CURVE_POINT*)&Emitter_LFE_CurvePoints[0], 3 };

    constexpr X3DAUDIO_DISTANCE_CURVE_POINT Emitter_Reverb_CurvePoints[3] = { { 0.0f, 0.5f }, { 0.75f, 1.0f }, { 1.0f, 0.0f } };
    constexpr X3DAUDIO_DISTANCE_CURVE       Emitter_Reverb_Curve = { (X3DAUDIO_DISTANCE_CURVE_POINT*)&Emitter_Reverb_CurvePoints[0], 3 };
#endif

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

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif

    static const wchar_t* s_searchFolders[] =
    {
        L"Audio3DTest",
        L"Tests\\Audio3DTest",
        nullptr
    };
}

Game::Game() noexcept(false) :
    m_critError(false),
    m_retrydefault(false),
    m_newAudio(false),
    m_frame(0),
    m_deviceStr{}
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

    // Set up 3D positional audio structures
    m_listener.SetCone(Listener_DirectionalCone);

    m_emitter.SetPosition(XMFLOAT3(10.f, 0.f, 0.f));
#ifdef USE_CUSTOM_CURVES
    m_emitter.pLFECurve = const_cast<X3DAUDIO_DISTANCE_CURVE*>(&Emitter_LFE_Curve);
    m_emitter.pReverbCurve = const_cast<X3DAUDIO_DISTANCE_CURVE*>(&Emitter_Reverb_Curve);
#else
    m_emitter.EnableDefaultCurves();
#endif
    m_emitter.CurveDistanceScaler = 14.f;
    m_emitter.SetCone(Emitter_DirectionalCone);
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

#ifdef USE_ZERO_CENTER
    eflags = eflags | AudioEngine_ZeroCenter3D;
#endif

#ifndef USE_DOPPLER
    eflags = eflags | AudioEngine_DisableDopplerEffect;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audEngine->SetReverb(Reverb_Default);

    SetDeviceString(m_audEngine.get(), m_deviceStr, 256);

    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"heli.wav", s_searchFolders);
    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), strFilePath);

    m_effect = m_soundEffect->CreateInstance(SoundEffectInstance_Use3D | SoundEffectInstance_ReverbUseFilters);
    if (!m_effect)
    {
        throw std::runtime_error("heli.wav");
    }

    m_effect->Play(true);

    // Try all the default multi-channel setups
    for (unsigned int j = 1; j < XAUDIO2_MAX_AUDIO_CHANNELS; ++j)
    {
        m_emitter.EnableDefaultMultiChannel(j);
    }

    // Set to the proper setup for this sound
    m_emitter.EnableDefaultMultiChannel(m_effect->GetChannelCount());

    UnitTests();
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

    AudioRender();

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

    float dt = static_cast<float>(timer.GetElapsedSeconds());

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

    assert(m_listener.IsValid());
    assert(m_emitter.IsValid());

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
    m_keyboardButtons.Reset();
}

void Game::OnSuspending()
{
    m_deviceResources->Suspend();

    m_audEngine->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_timer.ResetElapsedTime();
    m_keyboardButtons.Reset();
    m_audEngine->Resume();
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

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, 1);

#ifdef LH_COORDS
    m_sphere = GeometricPrimitive::CreateSphere(1.f, 16, false);
#else
    m_sphere = GeometricPrimitive::CreateSphere(1.f, 16);
#endif

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());
    {
        const EffectPipelineStateDescription pd(
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
        const SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }


    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"comic.spritefont", s_searchFolders);
    m_comicFont = std::make_unique<SpriteFont>(device, resourceUpload,
        strFilePath,
        m_resourceDescriptors->GetFirstCpuHandle(),
        m_resourceDescriptors->GetFirstGpuHandle());

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 14.f, 0.f, 0.f } } };

    const auto size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

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

    const auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);

#ifdef UWP
    const auto rotation = m_deviceResources->GetRotation();
    m_spriteBatch->SetRotation(rotation);

    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        m_projection *= orient;
    }
#endif
}

namespace DirectX
{
    // Internal function to validate
    uint32_t __cdecl GetDefaultChannelMask(int channels) noexcept;
}

void Game::UnitTests()
{
    bool success = true;
    OutputDebugStringA("*********** UNIT TESTS BEGIN ***************\n");

    // AudioListener
    {
        const XMVECTORF32 c_val = { { { 4.f, 5.f, 6.f } } };
        const XMVECTORF32 c_forward = { { { 10.f, 11.f, 12.f } } };
        const XMVECTORF32 c_top = { { { 13.f, 14.f, 15.f } } };

        const AudioListener def;
        if (!def.IsValid()
            || def.OrientFront.x != 0.f
            || def.OrientFront.y != 0.f
            || def.OrientFront.z != -1.f
            || def.OrientTop.x != 0.f
            || def.OrientTop.y != 1.f
            || def.OrientTop.z != 0.f
            || def.Position.x != 0.f
            || def.Position.y != 0.f
            || def.Position.z != 0.f
            || def.Velocity.x != 0.f
            || def.Velocity.y != 0.f
            || def.Velocity.z != 0.f
            || def.pCone != nullptr)
        {
            OutputDebugStringA("ERROR: Failed default listener tests\n");
            success = false;
        }

        AudioListener test = def;
        test.SetPosition(Vector3(1.f, 2.f, 3.f));

        AudioListener test2 = def;
        test2.SetPosition(c_val.v);

        if (!test.IsValid() || !test2.IsValid()
            || test.Position.x != 1.f
            || test.Position.y != 2.f
            || test.Position.z != 3.f
            || test2.Position.x != 4.f
            || test2.Position.y != 5.f
            || test2.Position.z != 6.f)
        {
            OutputDebugStringA("ERROR: Failed listener position tests\n");
            success = false;
        }

        test = def;
        test.SetVelocity(Vector3(1.f, 2.f, 3.f));

        test2 = def;
        test2.SetVelocity(c_val.v);

        if (!test.IsValid() || !test2.IsValid()
            || test.Velocity.x != 1.f
            || test.Velocity.y != 2.f
            || test.Velocity.z != 3.f
            || test2.Velocity.x != 4.f
            || test2.Velocity.y != 5.f
            || test2.Velocity.z != 6.f)
        {
            OutputDebugStringA("ERROR: Failed listener velocity tests\n");
            success = false;
        }

        test = def;
        test.SetOrientation(Vector3(10.f, 11.f, 12.f), Vector3(13.f, 14.f, 15.f));

        test2 = def;
        test2.SetOrientation(c_forward, c_top);

        if (!test.IsValid() || !test2.IsValid()
            || test.OrientFront.x != 10.f
            || test.OrientFront.y != 11.f
            || test.OrientFront.z != 12.f
            || test.OrientTop.x !=  13.f
            || test.OrientTop.y != 14.f
            || test.OrientTop.z != 15.f
            || test2.OrientFront.x != 10.f
            || test2.OrientFront.y != 11.f
            || test2.OrientFront.z != 12.f
            || test2.OrientTop.x !=  13.f
            || test2.OrientTop.y != 14.f
            || test2.OrientTop.z != 15.f)
        {
            OutputDebugStringA("ERROR: Failed listener orientation tests\n");
            success = false;
        }

        test = def;
        test.SetCone(Listener_DirectionalCone);

        test2 = def;
        test2.SetCone(Listener_DirectionalCone);
        test2.SetOmnidirectional();

        if (!test.IsValid() || !test2.IsValid()
            || test.pCone == nullptr
            || test2.pCone != nullptr)
        {
            OutputDebugStringA("ERROR: Failed listener cone tests\n");
            success = false;
        }

        test = def;
        test.OrientFront.z = NAN;

        test2 = def;
        test2.Position.x = NAN;

        AudioListener test3 = def;
        test3.Velocity.y = NAN;
        if (test.IsValid() || test2.IsValid() || test3.IsValid())
        {
            OutputDebugStringA("ERROR: Failed listener isvalid tests\n");
            success = false;
        }
    }

    // AudioEmitter
    {
        const XMVECTORF32 c_val = { { { 4.f, 5.f, 6.f } } };
        const XMVECTORF32 c_forward = { { { 10.f, 11.f, 12.f } } };
        const XMVECTORF32 c_top = { { { 13.f, 14.f, 15.f } } };

        const AudioEmitter def;
        if (!def.IsValid()
            || def.pCone != nullptr
            || def.OrientFront.x != 0.f
            || def.OrientFront.y != 0.f
            || def.OrientFront.z != -1.f
            || def.OrientTop.x != 0.f
            || def.OrientTop.y != 1.f
            || def.OrientTop.z != 0.f
            || def.Position.x != 0.f
            || def.Position.y != 0.f
            || def.Position.z != 0.f
            || def.Velocity.x != 0.f
            || def.Velocity.y != 0.f
            || def.Velocity.z != 0.f
            || def.InnerRadius != 0.f
            || def.InnerRadiusAngle == 0.f
            || def.ChannelCount != 1
            || def.ChannelRadius != 1.f
            || def.pChannelAzimuths == nullptr
            || def.pVolumeCurve != nullptr
            || def.pLFECurve != nullptr
            || def.pLPFDirectCurve != nullptr
            || def.pLPFReverbCurve != nullptr
            || def.pReverbCurve != nullptr
            || def.CurveDistanceScaler != 1.f
            || def.DopplerScaler != 1.f)
        {
            OutputDebugStringA("ERROR: Failed default emitter tests\n");
            success = false;
        }

        AudioEmitter test = def;
        test.SetPosition(Vector3(1.f, 2.f, 3.f));

        AudioEmitter test2 = def;
        test2.SetPosition(c_val.v);

        if (!test.IsValid() || !test2.IsValid()
            || test.Position.x != 1.f
            || test.Position.y != 2.f
            || test.Position.z != 3.f
            || test2.Position.x != 4.f
            || test2.Position.y != 5.f
            || test2.Position.z != 6.f)
        {
            OutputDebugStringA("ERROR: Failed emitter position tests\n");
            success = false;
        }

        test = def;
        test.SetVelocity(Vector3(1.f, 2.f, 3.f));

        test2 = def;
        test2.SetVelocity(c_val.v);

        if (!test.IsValid() || !test2.IsValid()
            || test.Velocity.x != 1.f
            || test.Velocity.y != 2.f
            || test.Velocity.z != 3.f
            || test2.Velocity.x != 4.f
            || test2.Velocity.y != 5.f
            || test2.Velocity.z != 6.f)
        {
            OutputDebugStringA("ERROR: Failed emitter velocity tests\n");
            success = false;
        }

        test = def;
        test.SetOrientation(Vector3(10.f, 11.f, 12.f), Vector3(13.f, 14.f, 15.f));

        test2 = def;
        test2.SetOrientation(c_forward, c_top);

        if (!test.IsValid() || !test2.IsValid()
            || test.OrientFront.x != 10.f
            || test.OrientFront.y != 11.f
            || test.OrientFront.z != 12.f
            || test.OrientTop.x !=  13.f
            || test.OrientTop.y != 14.f
            || test.OrientTop.z != 15.f
            || test2.OrientFront.x != 10.f
            || test2.OrientFront.y != 11.f
            || test2.OrientFront.z != 12.f
            || test2.OrientTop.x !=  13.f
            || test2.OrientTop.y != 14.f
            || test2.OrientTop.z != 15.f)
        {
            OutputDebugStringA("ERROR: Failed emitter orientation tests\n");
            success = false;
        }

        test = def;
        test.SetCone(Emitter_DirectionalCone);

        test2 = def;
        test2.SetCone(Emitter_DirectionalCone);
        test2.SetOmnidirectional();

        if (!test.IsValid() || !test2.IsValid()
            || test.pCone == nullptr
            || test2.pCone != nullptr)
        {
            OutputDebugStringA("ERROR: Failed emitter cone tests\n");
            success = false;
        }

        test = def;
        test.EnableDefaultCurves();

        test2 = def;
        test2.EnableInverseSquareCurves();

        AudioEmitter test3 = def;
        test3.EnableLinearCurves();

        if (!test.IsValid() || !test2.IsValid() || !test3.IsValid()
            || test.pVolumeCurve == nullptr
            || test2.pVolumeCurve != nullptr
            || test3.pVolumeCurve == nullptr
            || test.pLFECurve == nullptr
            || test2.pLFECurve != nullptr
            || test3.pLFECurve == nullptr
            || test.pLPFDirectCurve != nullptr
            || test2.pLPFDirectCurve != nullptr
            || test3.pLPFDirectCurve != nullptr
            || test.pLPFReverbCurve != nullptr
            || test2.pLPFReverbCurve != nullptr
            || test3.pLPFReverbCurve != nullptr
            || test.pReverbCurve != nullptr
            || test2.pReverbCurve != nullptr
            || test3.pReverbCurve != nullptr)
        {
            OutputDebugStringA("ERROR: Failed emitter curves tests\n");
            success = false;
        }

        test = def;
        test.OrientFront.z = NAN;

        test2 = def;
        test2.Position.x = NAN;

        test3 = def;
        test3.Velocity.y = NAN;
        if (test.IsValid() || test2.IsValid() || test3.IsValid())
        {
            OutputDebugStringA("ERROR: Failed emitter isvalid A tests\n");
            success = false;
        }

        test = def;
        test.ChannelCount = 0;

        test2 = def;
        test2.ChannelCount = XAUDIO2_MAX_AUDIO_CHANNELS;

        test3 = def;
        test3.ChannelCount = XAUDIO2_MAX_AUDIO_CHANNELS * 2;
        if (test.IsValid() || !test2.IsValid() || test3.IsValid())
        {
            OutputDebugStringA("ERROR: Failed emitter isvalid B tests\n");
            success = false;
        }

        test = def;
        test.InnerRadius = NAN;

        test2 = def;
        test2.InnerRadiusAngle = NAN;

        test3 = def;
        test3.ChannelCount = 4;
        test3.pChannelAzimuths = nullptr;
        if (test.IsValid() || test2.IsValid() || test3.IsValid())
        {
            OutputDebugStringA("ERROR: Failed emitter isvalid C tests\n");
            success = false;
        }

        test = def;
        test.ChannelRadius = NAN;

        test2 = def;
        test2.CurveDistanceScaler = NAN;

        test3 = def;
        test3.DopplerScaler = NAN;
        if (test.IsValid() || test2.IsValid() || test3.IsValid())
        {
            OutputDebugStringA("ERROR: Failed emitter isvalid D tests\n");
            success = false;
        }
    }

    // GetDefaultChannelMask
    {
        for (int j = 0; j < 16; ++j)
        {
            uint32_t mask = GetDefaultChannelMask(j);
            if (j == 0 || j > 8)
            {
                if (mask != 0)
                {
                    OutputDebugStringA("ERROR: Failed def channel mask A tests\n");
                    success = false;
                    break;
                }
            }
            else
            {
                if (mask == 0)
                {
                    OutputDebugStringA("ERROR: Failed def channel mask B tests\n");
                    success = false;
                    break;
                }
            }
        }
    }

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");

    if (!success)
    {
        throw std::runtime_error("Unit Tests Failed");
    }
}

#ifdef LOSTDEVICE
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
