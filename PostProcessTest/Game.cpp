//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK PostProcess
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr int MaxScene = 27;

    constexpr float ADVANCE_TIME = 1.f;
    constexpr float INTERACTIVE_TIME = 10.f;

    constexpr DXGI_FORMAT c_sdrFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    constexpr DXGI_FORMAT c_hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
}

//--------------------------------------------------------------------------------------

static_assert(std::is_nothrow_move_constructible<BasicPostProcess>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<BasicPostProcess>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<DualPostProcess>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DualPostProcess>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ToneMapPostProcess>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ToneMapPostProcess>::value, "Move Assign.");

//--------------------------------------------------------------------------------------

Game::Game() noexcept(false)  :
    m_scene(0),
    m_delay(0)
{
#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_sdrFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_sdrFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_sdrFormat, DXGI_FORMAT_D32_FLOAT);
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

    m_delay = ADVANCE_TIME;
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

    auto pad = m_gamePad->GetState(0);
    auto kb = m_keyboard->GetState();
    if (kb.Escape || (pad.IsConnected() && pad.IsViewPressed()))
    {
        ExitGame();
    }

    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED
            || m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
        {
            m_delay = INTERACTIVE_TIME;

            ++m_scene;
            if (m_scene >= MaxScene)
                m_scene = 0;
        }
        else if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED
            || m_gamePadButtons.dpadLeft == GamePad::ButtonStateTracker::PRESSED)
        {
            m_delay = INTERACTIVE_TIME;

            --m_scene;
            if (m_scene < 0)
                m_scene = MaxScene - 1;
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    m_keyboardButtons.Update(kb);

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        m_delay = INTERACTIVE_TIME;

        ++m_scene;
        if (m_scene >= MaxScene)
            m_scene = 0;
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::Back))
    {
        m_delay = INTERACTIVE_TIME;

        --m_scene;
        if (m_scene < 0)
            m_scene = MaxScene - 1;
    }

    float time = float(timer.GetTotalSeconds());

    m_delay -= static_cast<float>(timer.GetElapsedSeconds());

    if (m_delay <= 0.f)
    {
        m_delay = ADVANCE_TIME;

        ++m_scene;
        if (m_scene >= MaxScene)
            m_scene = 0;
    }

    m_world = Matrix::CreateRotationY(time);

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

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    auto size = m_deviceResources->GetOutputSize();

    m_spriteBatch->Begin(commandList);
    m_spriteBatch->Draw(
        m_resourceDescriptors->GetGpuHandle(m_scene >= 16 ? Descriptors::HDRTexture : Descriptors::Background),
        GetTextureSize(m_scene >= 16 ? m_hdrTexture.Get() : m_background.Get()),
        size);
    m_spriteBatch->End();

    m_effect->SetMatrices(m_world, m_view, m_proj);
    m_effect->Apply(commandList);
    m_shape->Draw(commandList);

    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_sceneTex.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
        commandList->ResourceBarrier(1, &barrier);
    }

    {
        auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
        commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
    }

    // Post process.
    const wchar_t* descstr = nullptr;
    switch (m_scene)
    {
    case 0:
    default:
        {
            descstr = L"Copy (passthrough)";
            auto pp = m_basicPostProcess[BasicPostProcess::Copy].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->Process(commandList);
        }
        break;

    case 1:
        {
            descstr = L"Monochrome";
            auto pp = m_basicPostProcess[BasicPostProcess::Monochrome].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->Process(commandList);
        }
        break;

    case 2:
        {
            descstr = L"Sepia";
            auto pp = m_basicPostProcess[BasicPostProcess::Sepia].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->Process(commandList);
        }
        break;

    case 3:
        {
            descstr = L"Downscale 2x2";
            auto pp = m_basicPostProcess[BasicPostProcess::DownScale_2x2].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->Process(commandList);
        }
        break;

    case 4:
        {
            descstr = L"Downscale 4x4";
            auto pp = m_basicPostProcess[BasicPostProcess::DownScale_4x4].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->Process(commandList);
        }
        break;

    case 5:
        {
            descstr = L"GaussianBlur 5x5";
            auto pp = m_basicPostProcess[BasicPostProcess::GaussianBlur_5x5].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetGaussianParameter(1.f);
            pp->Process(commandList);
        }
        break;

    case 6:
        {
            descstr = L"GaussianBlur 5x5 (2X)";
            auto pp = m_basicPostProcess[BasicPostProcess::GaussianBlur_5x5].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetGaussianParameter(2.f);
            pp->Process(commandList);
        }
        break;

    case 7:
        {
            descstr = L"BloomExtract";
            auto pp = m_basicPostProcess[BasicPostProcess::BloomExtract].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetBloomExtractParameter(0.25f);
            pp->Process(commandList);
        }
        break;

    case 8:
        {
            descstr = L"BloomBlur (extract + horizontal)";

            // Pass 1 (scene -> blur1)
            auto pp = m_basicPostProcess[BasicPostProcess::BloomExtract].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetBloomExtractParameter(0.25f);

            auto blurRT1 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur1RT);
            commandList->OMSetRenderTargets(1, &blurRT1, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 2 (blur1 -> rt)
            pp = m_basicPostProcess[BasicPostProcess::BloomBlur].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur1Tex), m_blur1Tex.Get());
            pp->SetBloomBlurParameters(true, 4.f, 1.f);

            auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
            commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_RENDER_TARGET, 0);
                commandList->ResourceBarrier(1, &barrier);
            }
        }
        break;

    case 9:
        {
            descstr = L"BloomBlur (extract + vertical)";

            // Pass 1 (scene -> blur1)
            auto pp = m_basicPostProcess[BasicPostProcess::BloomExtract].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetBloomExtractParameter(0.25f);

            auto blurRT1 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur1RT);
            commandList->OMSetRenderTargets(1, &blurRT1, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 2 (blur1 -> rt)
            pp = m_basicPostProcess[BasicPostProcess::BloomBlur].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur1Tex), m_blur1Tex.Get());
            pp->SetBloomBlurParameters(false, 4.f, 1.f);

            auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
            commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_RENDER_TARGET, 0);
                commandList->ResourceBarrier(1, &barrier);
            }
        }
        break;

    case 10:
        {
            descstr = L"BloomBlur (extract + horz + vert)";

            // Pass 1 (scene -> blur1)
            auto pp = m_basicPostProcess[BasicPostProcess::BloomExtract].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetBloomExtractParameter(0.25f);

            auto blurRT1 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur1RT);
            commandList->OMSetRenderTargets(1, &blurRT1, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 2 (blur1 -> blur2)
            pp = m_basicPostProcess[BasicPostProcess::BloomBlur].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur1Tex), m_blur1Tex.Get());
            pp->SetBloomBlurParameters(true, 4.f, 1.f);

            auto blurRT2 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur2RT);
            commandList->OMSetRenderTargets(1, &blurRT2, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur2Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 3 (blur2 -> rt)
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur2Tex), m_blur2Tex.Get());
            pp->SetBloomBlurParameters(false, 4.f, 1.f);

            auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
            commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

            pp->Process(commandList);

            {
                CD3DX12_RESOURCE_BARRIER barriers[2] =
                {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur2Tex.Get(),
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                };
                commandList->ResourceBarrier(2, barriers);
            }
        }
        break;

    case 11:
        {
            descstr = L"Bloom";

            // Pass 1 (scene -> blur1)
            auto pp = m_basicPostProcess[BasicPostProcess::BloomExtract].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetBloomExtractParameter(0.25f);

            auto blurRT1 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur1RT);
            commandList->OMSetRenderTargets(1, &blurRT1, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 2 (blur1 -> blur2)
            pp = m_basicPostProcess[BasicPostProcess::BloomBlur].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur1Tex), m_blur1Tex.Get());
            pp->SetBloomBlurParameters(true, 4.f, 1.f);

            auto blurRT2 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur2RT);
            commandList->OMSetRenderTargets(1, &blurRT2, FALSE, nullptr);

            pp->Process(commandList);

            {
                CD3DX12_RESOURCE_BARRIER barriers[2] =
                {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur2Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0),
                };
                commandList->ResourceBarrier(2, barriers);
            }

            // Pass 3 (blur2 -> blur1)
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur2Tex), m_blur2Tex.Get());
            pp->SetBloomBlurParameters(false, 4.f, 1.f);

            commandList->OMSetRenderTargets(1, &blurRT1, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 4 (scene+blur1 -> rt)
            auto dp = m_dualPostProcess[DualPostProcess::BloomCombine].get();
            dp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            dp->SetSourceTexture2(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur1Tex));
            dp->SetBloomCombineParameters(1.25f, 1.f, 1.f, 1.f);

            auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
            commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

            dp->Process(commandList);

            {
                CD3DX12_RESOURCE_BARRIER barriers[2] =
                {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur2Tex.Get(),
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                };
                commandList->ResourceBarrier(2, barriers);
            }
        }
        break;

    case 12:
        {
            descstr = L"Bloom (Saturated)";

            // Pass 1 (scene -> blur1)
            auto pp = m_basicPostProcess[BasicPostProcess::BloomExtract].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex), m_sceneTex.Get());
            pp->SetBloomExtractParameter(0.25f);

            auto blurRT1 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur1RT);
            commandList->OMSetRenderTargets(1, &blurRT1, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 2 (blur1 -> blur2)
            pp = m_basicPostProcess[BasicPostProcess::BloomBlur].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur1Tex), m_blur1Tex.Get());
            pp->SetBloomBlurParameters(true, 4.f, 1.f);

            auto blurRT2 = m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur2RT);
            commandList->OMSetRenderTargets(1, &blurRT2, FALSE, nullptr);

            pp->Process(commandList);

            {
                CD3DX12_RESOURCE_BARRIER barriers[2] =
                {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur2Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0),
                };
                commandList->ResourceBarrier(2, barriers);
            }

            // Pass 3 (blur2 -> blur1)
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur2Tex), m_blur2Tex.Get());
            pp->SetBloomBlurParameters(false, 4.f, 1.f);

            commandList->OMSetRenderTargets(1, &blurRT1, FALSE, nullptr);

            pp->Process(commandList);

            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
                commandList->ResourceBarrier(1, &barrier);
            }

            // Pass 4 (scene+blur1 -> rt)
            auto dp = m_dualPostProcess[DualPostProcess::BloomCombine].get();
            dp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            dp->SetSourceTexture2(m_resourceDescriptors->GetGpuHandle(Descriptors::Blur1Tex));
            dp->SetBloomCombineParameters(2.f, 1.f, 2.f, 0.f);

            auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
            commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

            dp->Process(commandList);

            {
                CD3DX12_RESOURCE_BARRIER barriers[2] =
                {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur1Tex.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_blur2Tex.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
                };
                commandList->ResourceBarrier(2, barriers);
            }
        }
        break;

    case 13:
        {
            descstr = L"Merge (90%/10%)";
            auto pp = m_dualPostProcess[DualPostProcess::Merge].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetSourceTexture2(m_resourceDescriptors->GetGpuHandle(Descriptors::HDRTexture));
            pp->SetMergeParameters(0.9f, 0.1f);
            pp->Process(commandList);
        }
        break;

    case 14:
        {
            descstr = L"Merge (50%/%50%)";
            auto pp = m_dualPostProcess[DualPostProcess::Merge].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetSourceTexture2(m_resourceDescriptors->GetGpuHandle(Descriptors::HDRTexture));
            pp->SetMergeParameters(0.5f, 0.5f);
            pp->Process(commandList);
        }
        break;

    case 15:
        {
            descstr = L"Merge (10%/%90%)";
            auto pp = m_dualPostProcess[DualPostProcess::Merge].get();
            pp->SetSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetSourceTexture2(m_resourceDescriptors->GetGpuHandle(Descriptors::HDRTexture));
            pp->SetMergeParameters(0.1f, 0.9f);
            pp->Process(commandList);
        }
        break;
       
    case 16:
        {
            descstr = L"ToneMap (None)";
            auto pp = m_toneMapPostProcess[0].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->Process(commandList);
        }
        break;

    case 17:
        {
            descstr = L"ToneMap (Saturate)";
            auto pp = m_toneMapPostProcess[3].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->Process(commandList);
        }
        break;

    case 18:
        {
            descstr = L"ToneMap (Reinhard)";
            auto pp = m_toneMapPostProcess[6].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->Process(commandList);
        }
        break;

    case 19:
        {
            descstr = L"ToneMap (ACES Filmic)";
            auto pp = m_toneMapPostProcess[9].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->Process(commandList);
        }
        break;

    case 20:
        {
            descstr = L"ToneMap (SRGB)";
            auto pp = m_toneMapPostProcess[1].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->Process(commandList);
        }
        break;

    case 21:
        {
            descstr = L"ToneMap (Saturate SRGB)";
            auto pp = m_toneMapPostProcess[4].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetExposure(0.f);
            pp->Process(commandList);
        }
        break;

    case 22:
        {
            descstr = L"ToneMap (Reinhard SRGB)";
            auto pp = m_toneMapPostProcess[7].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetExposure(0.f);
            pp->Process(commandList);
        }
        break;

    case 23:
        {
            descstr = L"ToneMap (ACES Filmic SRGB)";
            auto pp = m_toneMapPostProcess[10].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetExposure(0.f);
            pp->Process(commandList);
        }
        break;

    case 24:
        {
            descstr = L"ToneMap (Saturate SRGB EV 2)";
            auto pp = m_toneMapPostProcess[4].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetExposure(2.f);
            pp->Process(commandList);
        }
        break;

    case 25:
        {
            descstr = L"ToneMap (Reinhard SRGB EV 2)";
            auto pp = m_toneMapPostProcess[7].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetExposure(2.f);
            pp->Process(commandList);
        }
        break;

    case 26:
        {
            descstr = L"ToneMap (ACES Filmic SRGB, EV 2)";
            auto pp = m_toneMapPostProcess[10].get();
            pp->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
            pp->SetExposure(2.f);
            pp->Process(commandList);
        }
        break;

        // ST2084 scenarios and MRT are convered in the HDRTest
    }

    // Draw UI.
    auto safeRect = Viewport::ComputeTitleSafeArea(UINT(size.right), UINT(size.bottom));

    m_spriteBatchUI->Begin(commandList);
    m_font->DrawString(m_spriteBatchUI.get(), descstr, XMFLOAT2(float(safeRect.left), float(safeRect.bottom - m_font->GetLineSpacing())));
    m_spriteBatchUI->End();

    PIXEndEvent(commandList);

    // Set scene texture for next frame
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_sceneTex.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET, 0);
        commandList->ResourceBarrier(1, &barrier);
    }

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
    auto rtvDescriptor = m_rtvDescriptors->GetCpuHandle(RTDescriptors::SceneRT);
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    m_spriteBatch->SetViewport(viewport);
    m_spriteBatchUI->SetViewport(viewport);

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
    m_deviceResources->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
    m_timer.ResetElapsedTime();
}

#ifdef PC
void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
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

    RenderTargetState hdrState(c_hdrFormat, m_deviceResources->GetDepthBufferFormat());

    RenderTargetState rtState(c_sdrFormat, m_deviceResources->GetDepthBufferFormat());

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

    m_rtvDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    {
        SpriteBatchPipelineStateDescription pd(hdrState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    {
        SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatchUI = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    m_font = std::make_unique<SpriteFont>(device, resourceUpload, L"comic.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::Font),
        m_resourceDescriptors->GetGpuHandle(Descriptors::Font));

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"earth.bmp", m_texture.ReleaseAndGetAddressOf())
    );

    CreateShaderResourceView(device, m_texture.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Texture));

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"sunset.jpg", m_background.ReleaseAndGetAddressOf())
    );

    CreateShaderResourceView(device, m_background.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Background));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"HDR_029_Sky_Cloudy_Refbc6h.DDS", m_hdrTexture.ReleaseAndGetAddressOf())
    );

    CreateShaderResourceView(device, m_hdrTexture.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::HDRTexture));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();

    // Create scene objects
    m_states = std::make_unique<CommonStates>(device);

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            hdrState);

        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pd);
        m_effect->EnableDefaultLighting();

        m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Texture), m_states->LinearClamp());
    }

    m_shape = GeometricPrimitive::CreateTeapot();

    m_world = Matrix::Identity;

    // Setup post processing
    m_abstractPostProcess = std::make_unique<BasicPostProcess>(device, rtState, BasicPostProcess::Copy);

    for (int j = 0; j < static_cast<int>(BasicPostProcess::Effect_Max); ++j)
    {
        m_basicPostProcess[j] = std::make_unique<BasicPostProcess>(device, rtState, static_cast<BasicPostProcess::Effect>(j));
    }

    for (int j = 0; j < static_cast<int>(DualPostProcess::Effect_Max); ++j)
    {
        m_dualPostProcess[j] = std::make_unique<DualPostProcess>(device, rtState, static_cast<DualPostProcess::Effect>(j));
    }

    size_t j = 0;
#ifdef XBOX
    for (int m = 0; m < 2; ++m)
#else
    for (int m = 0; m < 1; ++m)
#endif
    {
        for (int op = 0; op < static_cast<int>(ToneMapPostProcess::Operator_Max); ++op)
        {
            for (int tf = 0; tf < static_cast<int>(ToneMapPostProcess::TransferFunction_Max); ++tf)
            {
                assert(j < ToneMapCount);
                _Analysis_assume_(j < ToneMapCount);
                m_toneMapPostProcess[j] = std::make_unique<ToneMapPostProcess>(device, rtState,
                    static_cast<ToneMapPostProcess::Operator>(op),
                    static_cast<ToneMapPostProcess::TransferFunction>(tf)
#ifdef XBOX
                    , (m > 0)
#endif
                    );
                ++j;
            }
        }
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();

    auto width = UINT(size.right - size.left);
    auto height = UINT(size.bottom - size.top);

    // Create scene render target
    auto device = m_deviceResources->GetD3DDevice();

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    {
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(c_hdrFormat,
            width, height,
            1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

        D3D12_CLEAR_VALUE clearValue = { c_hdrFormat, {} };
        memcpy(clearValue.Color, Colors::CornflowerBlue.f, sizeof(clearValue.Color));

        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
                &desc,
                D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
                IID_GRAPHICS_PPV_ARGS(m_sceneTex.ReleaseAndGetAddressOf()))
        );

        device->CreateRenderTargetView(m_sceneTex.Get(), nullptr, m_rtvDescriptors->GetCpuHandle(RTDescriptors::SceneRT));

        device->CreateShaderResourceView(m_sceneTex.Get(), nullptr, m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex));
    }

    // Create additional render targets
    {
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(c_sdrFormat,
            width, height,
            1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

        D3D12_CLEAR_VALUE clearValue = { c_sdrFormat, {} };
        memcpy(clearValue.Color, Colors::CornflowerBlue.f, sizeof(clearValue.Color));

        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
                &desc,
                D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
                IID_GRAPHICS_PPV_ARGS(m_blur1Tex.ReleaseAndGetAddressOf()))
        );

        device->CreateRenderTargetView(m_blur1Tex.Get(), nullptr,
            m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur1RT));

        device->CreateShaderResourceView(m_blur1Tex.Get(), nullptr, m_resourceDescriptors->GetCpuHandle(Descriptors::Blur1Tex));
    
        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
                &desc,
                D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
                IID_GRAPHICS_PPV_ARGS(m_blur2Tex.ReleaseAndGetAddressOf()))
        );

        device->CreateRenderTargetView(m_blur2Tex.Get(), nullptr,
            m_rtvDescriptors->GetCpuHandle(RTDescriptors::Blur2RT));

        device->CreateShaderResourceView(m_blur2Tex.Get(), nullptr, m_resourceDescriptors->GetCpuHandle(Descriptors::Blur2Tex));
    }

    // Setup matrices
    m_view = Matrix::CreateLookAt(Vector3(2.f, 2.f, 2.f),
        Vector3::Zero, Vector3::UnitY);

    m_proj = Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f,
        float(size.right) / float(size.bottom), 0.1f, 10.f);
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_resourceDescriptors.reset();
    m_rtvDescriptors.reset();

    m_spriteBatch.reset();
    m_spriteBatchUI.reset();
    m_font.reset();
    m_states.reset();
    m_effect.reset();
    m_shape.reset();

    m_texture.Reset();
    m_background.Reset();
    m_hdrTexture.Reset();

    m_sceneTex.Reset();
    m_blur1Tex.Reset();
    m_blur2Tex.Reset();

    m_abstractPostProcess.reset();

    for (int j = 0; j < static_cast<int>(BasicPostProcess::Effect_Max); ++j)
    {
        m_basicPostProcess[j].reset();
    }

    for (int j = 0; j < static_cast<int>(DualPostProcess::Effect_Max); ++j)
    {
        m_dualPostProcess[j].reset();
    }

    for (int j = 0; j < ToneMapCount; ++j)
    {
        m_toneMapPostProcess[j].reset();
    }

    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion
