//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK PBR Model Test
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include "FindMedia.h"

// Build for LH vs. RH coords
//#define LH_COORDS

// For UWP/PC, this tests using a linear F16 swapchain intead of HDR10
//#define TEST_HDR_LINEAR

#define REVERSEZ

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float rowtop = 6.f;
    constexpr float row0 = 1.5f;
    constexpr float row1 = 0.f;
    constexpr float row2 = -1.5f;

    constexpr XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
}

// Constructor.
Game::Game() noexcept(false) :
    m_instanceCount(0),
    m_ibl(0),
    m_spinning(true),
    m_pitch(0),
    m_yaw(0),
    m_frame(0)
{
#if defined(TEST_HDR_LINEAR) && !defined(XBOX)
    constexpr DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
#else
    constexpr DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
#endif

    unsigned int deviceOptions = DX::DeviceResources::c_EnableHDR;
#ifdef REVERSEZ
    deviceOptions |= DX::DeviceResources::c_ReverseDepth;
#endif

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
        deviceOptions
        );
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
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

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

        if (m_gamePadButtons.dpadDown == GamePad::ButtonStateTracker::PRESSED
            || m_gamePadButtons.dpadLeft == GamePad::ButtonStateTracker::PRESSED)
        {
            ++m_ibl;
            if (m_ibl >= s_nIBL)
            {
                m_ibl = 0;
            }
        }
        else if (m_gamePadButtons.dpadUp == GamePad::ButtonStateTracker::PRESSED
            || m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
        {
            if (!m_ibl)
                m_ibl = s_nIBL - 1;
            else
                --m_ibl;
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

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Enter) && !kb.LeftAlt && !kb.RightAlt)
    {
        ++m_ibl;
        if (m_ibl >= s_nIBL)
        {
            m_ibl = 0;
        }
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::Back))
    {
        if (!m_ibl)
            m_ibl = s_nIBL - 1;
        else
            --m_ibl;
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        m_spinning = !m_spinning;
    }

    m_teapotAnim.Update(elapsedTime);

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

    auto time = static_cast<float>(m_timer.GetTotalSeconds());

    float alphaFade = (sin(time * 2) + 1) / 2;

    if (alphaFade >= 1)
        alphaFade = 1 - FLT_EPSILON;

    float yaw = time * 1.4f;

    XMMATRIX world;

    if (m_spinning)
    {
        world = XMMatrixRotationRollPitchYaw(0, yaw, 0);
    }
    else
    {
        world = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0);
    }

    auto commandList = m_deviceResources->GetCommandList();
    m_hdrScene->BeginScene(commandList);

    Clear();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    //--- Set PBR lighting sources ---
    const auto radianceTex = m_resourceDescriptors->GetGpuHandle(Descriptors::RadianceIBL1 + size_t(m_ibl));

#ifdef __MINGW32__
    D3D12_RESOURCE_DESC diffuseDesc;
    std::ignore = m_radianceIBL[0]->GetDesc(&diffuseDesc);
#else
    const auto diffuseDesc = m_radianceIBL[0]->GetDesc();
#endif

    const auto irradianceTex = m_resourceDescriptors->GetGpuHandle(Descriptors::IrradianceIBL1 + size_t(m_ibl));

    for (auto& it : m_cubeNormal)
    {
        auto pbr = dynamic_cast<PBREffect*>(it.get());
        if (pbr)
        {
            pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
        }
    }

    for (auto& it : m_cubeInstNormal)
    {
        auto pbr = dynamic_cast<PBREffect*>(it.get());
        if (pbr)
        {
            pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
        }
    }

    for (auto& it : m_sphereNormal)
    {
        auto pbr = dynamic_cast<PBREffect*>(it.get());
        if (pbr)
        {
            pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
        }
    }

    for (auto& it : m_sphere2Normal)
    {
        auto pbr = dynamic_cast<PBREffect*>(it.get());
        if (pbr)
        {
            pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
        }
    }

    for (auto& it : m_robotNormal)
    {
        auto pbr = dynamic_cast<PBREffect*>(it.get());
        if (pbr)
        {
            pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
        }
    }

    for (auto& it : m_teapotNormal)
    {
        auto pbr = dynamic_cast<PBREffect*>(it.get());
        if (pbr)
        {
            pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
        }
    }

    //--- Draw SDKMESH models ---
    XMMATRIX local = XMMatrixTranslation(1.5f, row0, 0.f);
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_cubeNormal, local, m_view, m_projection);
    m_cube->Draw(commandList, m_cubeNormal.cbegin());

    {
        XMMATRIX scale = XMMatrixScaling(0.75f, 0.75f, 0.75f);
        XMMATRIX trans = XMMatrixTranslation(-2.5f, row0, 0.f);
        local = XMMatrixMultiply(scale, trans);
        local = XMMatrixMultiply(world, local);
    }
    Model::UpdateEffectMatrices(m_sphereNormal, local, m_view, m_projection);
    m_sphere->Draw(commandList, m_sphereNormal.cbegin());

    {
        XMMATRIX scale = XMMatrixScaling(0.75f, 0.75f, 0.75f);
        XMMATRIX trans = XMMatrixTranslation(-2.5f, row2, 0.f);
        local = XMMatrixMultiply(scale, trans);
        local = XMMatrixMultiply(world, local);
    }
    Model::UpdateEffectMatrices(m_sphere2Normal, local, m_view, m_projection);
    m_sphere2->Draw(commandList, m_sphere2Normal.cbegin());

    {
        XMMATRIX scale = XMMatrixScaling(0.1f, 0.1f, 0.1f);
        XMMATRIX trans = XMMatrixTranslation(1.5f, row2, 0.f);
        local = XMMatrixMultiply(scale, trans);
        local = XMMatrixMultiply(world, local);
    }
    Model::UpdateEffectMatrices(m_robotNormal, local, m_view, m_projection);
    m_robot->Draw(commandList, m_robotNormal.cbegin());

    //--- Draw with instancing ---
    local = XMMatrixTranslation(0.f, rowtop, 0.f) * XMMatrixScaling(0.25f, 0.25f, 0.25f);
    {
        size_t j = 0;
        for (float x = -16.f; x <= 16.f; x += 4.f)
        {
            XMMATRIX m = world * XMMatrixTranslation(x, rowtop, cos(time + float(j) * XM_PIDIV4));
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

        for (const auto& mit : m_cubeInst->meshes)
        {
            auto mesh = mit.get();
            assert(mesh != 0);

            for (const auto& it : mesh->opaqueMeshParts)
            {
                auto part = it.get();
                assert(part != 0);

                auto effect = m_cubeInstNormal[part->partIndex].get();

                auto imatrices = dynamic_cast<IEffectMatrices*>(effect);
                if (imatrices) imatrices->SetMatrices(local, m_view, m_projection);

                effect->Apply(commandList);
                part->DrawInstanced(commandList, m_instanceCount);
            }

            // Skipping alphaMeshParts for this model since we know it's empty...
            assert(mesh->alphaMeshParts.empty());
        }
    }

    //--- Draw with skinning ---
    const auto nbones = static_cast<uint32_t>(m_teapot->bones.size());
    const auto bones = ModelBone::MakeArray(nbones);
    m_teapotAnim.Apply(*m_teapot, m_teapot->bones.size(), bones.get());

    local = XMMatrixMultiply(XMMatrixScaling(0.02f, 0.02f, 0.02f), XMMatrixTranslation(4.f, row1, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_teapotNormal, local, m_view, m_projection);
    m_teapot->DrawSkinned(commandList, nbones, bones.get(), local, m_teapotNormal.cbegin());

    PIXEndEvent(commandList);

    // Tonemap the frame.
    m_hdrScene->EndScene(commandList);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Tonemap");

#ifdef XBOX
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptors[2] = { m_deviceResources->GetRenderTargetView(), m_deviceResources->GetGameDVRRenderTargetView() };
    commandList->OMSetRenderTargets(2, rtvDescriptors, FALSE, nullptr);

    m_toneMap->Process(commandList);
#else
    const auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
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

#ifdef REVERSEZ
    constexpr float c_zclear = 0.f;
#else
    constexpr float c_zclear = 1.f;
#endif

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, c_clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, c_zclear, 0, 0, nullptr);

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

    m_resourceDescriptors = std::make_unique<DescriptorPile>(device,
        Descriptors::Count,
        Descriptors::Reserve);

    m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_states = std::make_unique<CommonStates>(device);

    m_hdrScene->SetDevice(device,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene));

    // DirectX SDK Mesh
    m_cube = Model::CreateFromSDKMESH(device, L"BrokenCube.sdkmesh");
    m_sphere = Model::CreateFromSDKMESH(device, L"Sphere.sdkmesh");
    m_sphere2 = Model::CreateFromSDKMESH(device, L"Sphere2.sdkmesh");
    m_robot = Model::CreateFromSDKMESH(device, L"ToyRobot.sdkmesh");

    // Create instanced mesh.
    m_cubeInst = Model::CreateFromSDKMESH(device, L"BrokenCube.sdkmesh");

    static const D3D12_INPUT_ELEMENT_DESC s_instElements[] =
    {
        // XMFLOAT3X4
        { "InstMatrix",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",  2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    for (auto& mit : m_cubeInst->meshes)
    {
        auto mesh = mit.get();
        assert(mesh != 0);

        for (auto& it : mesh->opaqueMeshParts)
        {
            auto part = it.get();
            assert(part != 0);

            auto il = *part->vbDecl;
            il.push_back(s_instElements[0]);
            il.push_back(s_instElements[1]);
            il.push_back(s_instElements[2]);

            it->vbDecl = std::make_unique<ModelMeshPart::InputLayoutCollection>(il);
        }

        // Skipping alphaMeshParts for this model since we know it's empty...
        assert(mesh->alphaMeshParts.empty());
    }

    // Create skinning teapot.
    size_t animsOffset;
    m_teapot = Model::CreateFromCMO(device, L"teapot.cmo", ModelLoader_IncludeBones, &animsOffset);

    if (!animsOffset)
    {
        OutputDebugStringA("ERROR: 'teapot.cmo' - No animation clips found in file!\n");
    }
    else
    {
        DX::ThrowIfFailed(m_teapotAnim.Load(L"teapot.cmo", animsOffset, L"Take 001"));

        OutputDebugStringA("'teapot.cmo' contains animation clips.\n");
    }

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
#ifdef XBOX
    rtState.numRenderTargets = 2;
    rtState.rtvFormats[1] = m_deviceResources->GetGameDVRFormat();
#endif

    m_toneMap = std::make_unique<ToneMapPostProcess>(
        device,
        rtState,
        ToneMapPostProcess::Reinhard, (m_deviceResources->GetBackBufferFormat() == DXGI_FORMAT_R16G16B16A16_FLOAT) ? ToneMapPostProcess::Linear : ToneMapPostProcess::SRGB
#ifdef XBOX
        , true
#endif
        );

#ifndef XBOX
    m_toneMapLinear = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::Linear);
    m_toneMapHDR10 = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::ST2084);
#endif

    // Load textures
    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    m_modelResources = std::make_unique<EffectTextureFactory>(device, resourceUpload, m_resourceDescriptors->Heap());

    m_fxFactory = std::make_unique<PBREffectFactory>(m_resourceDescriptors->Heap(), m_states->Heap());

    RenderTargetState hdrState(m_hdrScene->GetFormat(), m_deviceResources->GetDepthBufferFormat());

#ifdef LH_COORDS
    const auto& ncull = CommonStates::CullCounterClockwise;
    const auto& cull = CommonStates::CullClockwise;
#else
    const auto& ncull = CommonStates::CullClockwise;
    const auto& cull = CommonStates::CullCounterClockwise;
#endif

#ifdef REVERSEZ
    const auto& c_depthState = CommonStates::DepthReverseZ;
#else
    const auto& c_depthState = CommonStates::DepthDefault;
#endif

    EffectPipelineStateDescription pd(
        nullptr,
        CommonStates::Opaque,
        c_depthState,
        ncull,
        hdrState);

    int txtOffset = 0;
    {
        size_t start, end;
        m_resourceDescriptors->AllocateRange(m_cube->textureNames.size(), start, end);
        txtOffset = static_cast<int>(start);
    }
    m_cube->LoadTextures(*m_modelResources, txtOffset);

    m_cubeNormal = m_cube->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

    {
        size_t start, end;
        m_resourceDescriptors->AllocateRange(m_sphere->textureNames.size(), start, end);
        txtOffset = static_cast<int>(start);
    }
    m_sphere->LoadTextures(*m_modelResources, txtOffset);

    m_sphereNormal = m_sphere->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

    {
        size_t start, end;
        m_resourceDescriptors->AllocateRange(m_sphere2->textureNames.size(), start, end);
        txtOffset = static_cast<int>(start);
    }
    m_sphere2->LoadTextures(*m_modelResources, txtOffset);

    m_sphere2Normal = m_sphere2->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

    {
        size_t start, end;
        m_resourceDescriptors->AllocateRange(m_robot->textureNames.size(), start, end);
        txtOffset = static_cast<int>(start);
    }
    m_robot->LoadTextures(*m_modelResources, txtOffset);

    m_robotNormal = m_robot->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

    {
        size_t start, end;
        m_resourceDescriptors->AllocateRange(m_cubeInst->textureNames.size(), start, end);
        txtOffset = static_cast<int>(start);
    }
    m_cubeInst->LoadTextures(*m_modelResources, txtOffset);

    m_fxFactory->EnableInstancing(true);
    m_cubeInstNormal = m_cubeInst->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

    pd.rasterizerDesc = cull;
    m_teapotNormal = m_teapot->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

    // Optimize VBs/IBs
    m_cube->LoadStaticBuffers(device, resourceUpload);
    m_sphere->LoadStaticBuffers(device, resourceUpload);
    m_sphere2->LoadStaticBuffers(device, resourceUpload);
    m_robot->LoadStaticBuffers(device, resourceUpload);
    m_teapot->LoadStaticBuffers(device, resourceUpload);

    static const wchar_t* s_radianceIBL[s_nIBL] =
    {
        L"Atrium_diffuseIBL.dds",
        L"Garage_diffuseIBL.dds",
        L"SunSubMixer_diffuseIBL.dds",
    };
    static const wchar_t* s_irradianceIBL[s_nIBL] =
    {
        L"Atrium_specularIBL.dds",
        L"Garage_specularIBL.dds",
        L"SunSubMixer_specularIBL.dds",
    };

    static_assert(std::size(s_radianceIBL) == std::size(s_irradianceIBL), "IBL array mismatch");

    static const wchar_t* s_searchFolders[] =
    {
        L"PBRModelTest",
        L"PBRTest",
        nullptr
    };

    for (size_t j = 0; j < s_nIBL; ++j)
    {
        wchar_t strFilePath[MAX_PATH] = {};
        DX::FindMediaFile(strFilePath, MAX_PATH, s_radianceIBL[j], s_searchFolders);
        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, strFilePath, m_radianceIBL[j].ReleaseAndGetAddressOf())
        );

        DX::FindMediaFile(strFilePath, MAX_PATH, s_irradianceIBL[j], s_searchFolders);
        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, strFilePath, m_irradianceIBL[j].ReleaseAndGetAddressOf())
        );

        CreateShaderResourceView(device, m_radianceIBL[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::RadianceIBL1 + j), true);
        CreateShaderResourceView(device, m_irradianceIBL[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::IrradianceIBL1 + j), true);
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    // Create instance transforms.
    {
        size_t j = 0;
        for (float x = -16.f; x <= 16.f; x += 4.f)
        {
            ++j;
        }
        m_instanceCount = static_cast<UINT>(j);

        m_instanceTransforms = std::make_unique<XMFLOAT3X4[]>(j);

        constexpr XMFLOAT3X4 s_identity = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f };

        j = 0;
        for (float x = -16.f; x <= 16.f; x += 4.f)
        {
            m_instanceTransforms[j] = s_identity;
            m_instanceTransforms[j]._14 = x;
            ++j;
        }
    }

    m_teapotAnim.Bind(*m_teapot);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 6.f, 0.f } } };

    const auto size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef REVERSEZ
    constexpr float c_nearz = 15.f;
    constexpr float c_farz = 1.f;
#else
    constexpr float c_nearz = 1.f;
    constexpr float c_farz = 15.f;
#endif

#ifdef LH_COORDS
    m_view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovLH(1, aspect, c_nearz, c_farz);
#else
    m_view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovRH(1, aspect, c_nearz, c_farz);
#endif

#ifdef UWP
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        m_projection *= orient;
    }
#endif

    // Set windows size for HDR.
    m_hdrScene->SetWindow(size);

    m_toneMap->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));

#ifndef XBOX
    m_toneMapLinear->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
    m_toneMapHDR10->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
#endif
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_cube.reset();
    m_cubeInst.reset();
    m_sphere.reset();
    m_sphere2.reset();
    m_robot.reset();
    m_teapot.reset();

    m_cubeNormal.clear();
    m_cubeInstNormal.clear();
    m_sphereNormal.clear();
    m_sphere2Normal.clear();
    m_robotNormal.clear();
    m_teapotNormal.clear();

    for (size_t j = 0; j < s_nIBL; ++j)
    {
        m_radianceIBL[j].Reset();
        m_irradianceIBL[j].Reset();
    }

    m_states.reset();

    m_toneMap.reset();
    m_toneMapLinear.reset();
    m_toneMapHDR10.reset();

    m_hdrScene->ReleaseDevice();

    m_modelResources.reset();
    m_fxFactory.reset();

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
