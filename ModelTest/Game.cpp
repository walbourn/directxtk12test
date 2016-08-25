//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Model
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

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

#define GAMMA_CORRECT_RENDERING
#define AUTOGENMIPS
#define PERPIXELLIGHTING
#define NORMALMAPS

// Build for LH vs. RH coords
#define LH_COORDS

std::unique_ptr<Model> CreateModelFromOBJ(_In_z_ const wchar_t* szFileName);

namespace
{
    const float row0 = 2.f;
    const float row1 = 0.f;
    const float row2 = -2.f;
}

Game::Game()
{
#ifdef GAMMA_CORRECT_RENDERING
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>();
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
    m_keyboard = std::make_unique<Keyboard>();

#if defined(_XBOX_ONE) && defined(_TITLE)
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
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

    m_bones.reset(reinterpret_cast<XMMATRIX*>(_aligned_malloc(sizeof(XMMATRIX) * SkinnedEffect::MaxBones, 16)));
    XMMATRIX id = XMMatrixIdentity();
    for (size_t j = 0; j < SkinnedEffect::MaxBones; ++j)
    {
        m_bones[j] = id;
    }
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

    auto kb = m_keyboard->GetState();

    if (kb.Escape)
    {
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
        PostQuitMessage(0);
#else
        Windows::ApplicationModel::Core::CoreApplication::Exit();
#endif
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

    XMMATRIX world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    XMVECTOR quat = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);

    // Skinning settings
    float s = 1 + sin(time * 1.7f) * 0.5f;

    XMMATRIX scale = XMMatrixScaling(s, s, s);

    for (size_t j = 0; j < SkinnedEffect::MaxBones; ++j)
    {
        m_bones[j] = scale;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    //--- Draw Wavefront OBJ models --------------------------------------------------------
    for (auto& it : m_cupNormal)
    {
        auto lights = dynamic_cast<IEffectLights*>(it.get());
        if (lights)
        {
            lights->EnableDefaultLighting();
        }
    }

    XMMATRIX local = XMMatrixTranslation(1.5f, row0, 0.f);
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_cupNormal, local, m_view, m_projection);
    m_cup->Draw(commandList, m_cupNormal.cbegin());

        // Wireframe
    local = XMMatrixTranslation(3.f, row0, 0.f);
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_cupWireframe, local, m_view, m_projection);
    m_cup->Draw(commandList, m_cupWireframe.cbegin());

        // Custom settings
    local = XMMatrixTranslation(0.f, row0, 0.f);
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_cupCustom, local, m_view, m_projection);
    m_cup->Draw(commandList, m_cupCustom.cbegin());

        // Lighting settings
    for (auto& it : m_cupNormal)
    {
        auto lights = dynamic_cast<IEffectLights*>(it.get());
        if (lights)
        {
            XMVECTOR dir = XMVector3Rotate(g_XMOne, quat);
            lights->SetLightDirection(0, dir);
        }
    }

    local = XMMatrixTranslation(-1.5f, row0, 0.f);
    Model::UpdateEffectMatrices(m_cupNormal, local, m_view, m_projection);
    m_cup->Draw(commandList, m_cupNormal.cbegin());

        // No per pixel lighting
    local = XMMatrixTranslation(-3.f, row0, 0.f);
    Model::UpdateEffectMatrices(m_cupVertexLighting, local, m_view, m_projection);
    for (auto& it : m_cupVertexLighting)
    {
        auto lights = dynamic_cast<IEffectLights*>(it.get());
        if (lights)
        {
            XMVECTOR dir = XMVector3Rotate(g_XMOne, quat);
            lights->SetLightDirection(0, dir);

        }
    }
    m_cup->Draw(commandList, m_cupVertexLighting.cbegin());

        // Fog settings
    local = XMMatrixTranslation(-4.f, row0, cos(time) * 2.f);
    Model::UpdateEffectMatrices(m_cupFog, local, m_view, m_projection);
    m_cup->Draw(commandList, m_cupFog.cbegin());

    //--- Draw VBO models ------------------------------------------------------------------
    local = XMMatrixMultiply(XMMatrixScaling(0.25f, 0.25f, 0.25f), XMMatrixTranslation(4.5f, row0, 0.f));
    local = XMMatrixMultiply(world, local);
    m_vboNormal->SetWorld(local);
    m_vbo->Draw(commandList, m_vboNormal.get());

    local = XMMatrixMultiply(XMMatrixScaling(0.25f, 0.25f, 0.25f), XMMatrixTranslation(4.5f, row2, 0.f));
    local = XMMatrixMultiply(world, local);
    m_vboEnvMap->SetWorld(local);
    m_vbo->Draw(commandList, m_vboEnvMap.get());

    //--- Draw SDKMESH models --------------------------------------------------------------
    local = XMMatrixTranslation(0.f, row2, 0.f);
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_cupMeshNormal, local, m_view, m_projection);
    m_cupMesh->Draw(commandList, m_cupMeshNormal.cbegin());

    local = XMMatrixMultiply(XMMatrixScaling(0.005f, 0.005f, 0.005f), XMMatrixTranslation(2.5f, row2, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_tinyNormal, local, m_view, m_projection);
    m_tiny->Draw(commandList, m_tinyNormal.cbegin());

    local = XMMatrixTranslation(-2.5f, row2, 0.f);
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_dwarfNormal, local, m_view, m_projection);
    m_dwarf->Draw(commandList, m_dwarfNormal.cbegin());

    local = XMMatrixMultiply(XMMatrixScaling(0.01f, 0.01f, 0.01f), XMMatrixTranslation(-5.0f, row2, 0.f));
    local = XMMatrixMultiply(XMMatrixRotationRollPitchYaw(0, XM_PI, roll), local);
    Model::UpdateEffectMatrices(m_lmapNormal, local, m_view, m_projection);
    m_lmap->Draw(commandList, m_lmapNormal.cbegin());

    for (auto& it : m_soldierNormal)
    {
        auto skin = dynamic_cast<IEffectSkinning*>(it.get());
        if (skin)
        {
            skin->ResetBoneTransforms();
        }
    }

    local = XMMatrixMultiply(XMMatrixScaling(2.f, 2.f, 2.f), XMMatrixTranslation(3.5f, row1, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_soldierNormal, local, m_view, m_projection);
    m_soldier->Draw(commandList, m_soldierNormal.cbegin());

    for (auto& it : m_soldierNormal)
    {
        auto skin = dynamic_cast<IEffectSkinning*>(it.get());
        if (skin)
        {
            skin->SetBoneTransforms(m_bones.get(), SkinnedEffect::MaxBones);
        }
    }

    local = XMMatrixMultiply(XMMatrixScaling(2.f, 2.f, 2.f), XMMatrixTranslation(2.5f, row1, 0.f));
    Model::UpdateEffectMatrices(m_soldierNormal, local, m_view, m_projection);
    m_soldier->Draw(commandList, m_soldierNormal.cbegin());

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
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);

    XMVECTORF32 color;
#ifdef GAMMA_CORRECT_RENDERING
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
#else
    color.v = Colors::CornflowerBlue;
#endif
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

    m_states = std::make_unique<CommonStates>(device);

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    m_cup = CreateModelFromOBJ(L"cup._obj");

    m_vbo = Model::CreateFromVBO(L"player_ship_a.vbo");

    // Load textures & effects
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    m_modelResources = std::make_unique<EffectTextureFactory>(device, resourceUpload, m_resourceDescriptors->Heap());
#ifdef GAMMA_CORRECT_RENDERING
    m_modelResources->EnableForceSRGB(true);
#endif

#ifdef AUTOGENMIPS
    m_modelResources->EnableAutoGenMips(true);
#endif

    m_fxFactory = std::make_unique<EffectFactory>(m_resourceDescriptors->Heap(), m_states->Heap());

#ifndef PERPIXELLIGHTING
    m_fxFactory->EnablePerPixelLighting(false);
#endif

#ifndef NORMALMAPS
    m_fxFactory->EnableNormalMapEffect(false);
#endif

    // Create cup materials & effects
    int txtOffset = Descriptors::ModelStart;
    int txtAdd = m_cup->LoadTextures(*m_modelResources, txtOffset);

#ifdef LH_COORDS
    auto& ncull = CommonStates::CullCounterClockwise;
    
#else
    auto& ncull = CommonStates::CullClockwise;
#endif

    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        EffectPipelineStateDescription wireframe(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::Wireframe,
            rtState);

        m_cupNormal = m_cup->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

        m_fxFactory->SetSharing(false);
        m_cupCustom = m_cup->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        {
            auto basic = dynamic_cast<BasicEffect*>(m_cupCustom[1].get());
            if (basic)
            {
                basic->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex), m_states->AnisotropicWrap());
            }
        }
        m_fxFactory->SetSharing(true);

        m_cupWireframe = m_cup->CreateEffects(*m_fxFactory, wireframe, wireframe, txtOffset);

        m_fxFactory->EnableFogging(true);

        m_cupFog = m_cup->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

        m_fxFactory->EnableFogging(false);

        m_fxFactory->EnablePerPixelLighting(false);

        m_cupVertexLighting = m_cup->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

        m_fxFactory->EnablePerPixelLighting(true);
    }

    txtOffset += txtAdd;

    // Create VBO effects (no textures)

    {
        EffectPipelineStateDescription pd(
            &VertexPositionNormalTexture::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        m_vboNormal = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pd);
        m_vboNormal->EnableDefaultLighting();

        m_vboEnvMap = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pd);
        m_vboEnvMap->EnableDefaultLighting();
    }

    // SDKMESH Cup
    m_cupMesh = Model::CreateFromSDKMESH(L"cup.sdkmesh");

    txtAdd = m_cupMesh->LoadTextures(*m_modelResources, txtOffset);

    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        m_cupMeshNormal = m_cupMesh->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
    }

    txtOffset += txtAdd;

    // SDKMESH Tiny
    m_tiny = Model::CreateFromSDKMESH(L"tiny.sdkmesh");

    txtAdd = m_tiny->LoadTextures(*m_modelResources, txtOffset);

    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        m_tinyNormal = m_tiny->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
    }

    txtOffset += txtAdd;

    // SDKMESH Soldier
    m_soldier = Model::CreateFromSDKMESH(L"soldier.sdkmesh");

    txtAdd = m_soldier->LoadTextures(*m_modelResources, txtOffset);

    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        m_soldierNormal = m_soldier->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
    }

    txtOffset += txtAdd;

    // SDKMESH Dwarf
    m_dwarf = Model::CreateFromSDKMESH(L"dwarf.sdkmesh");

    txtAdd = m_dwarf->LoadTextures(*m_modelResources, txtOffset);

    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        m_dwarfNormal = m_dwarf->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
    }

    txtOffset += txtAdd;

    // SDKMESH Lightmap
    m_lmap = Model::CreateFromSDKMESH(L"SimpleLightMap.sdkmesh");

    txtAdd = m_lmap->LoadTextures(*m_modelResources, txtOffset);

    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        m_lmapNormal = m_lmap->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
    }

    txtOffset += txtAdd;

#ifdef GAMMA_CORRECT_RENDERING
    bool forceSRGB = true;
#else
    bool forceSRGB = false;
#endif

    // Load test textures
    {
        DX::ThrowIfFailed(
            CreateDDSTextureFromFileEx(device, resourceUpload, L"default.dds",
                0, D3D12_RESOURCE_FLAG_NONE, forceSRGB, false,
                m_defaultTex.ReleaseAndGetAddressOf()));

        CreateShaderResourceView(device, m_defaultTex.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DefaultTex));

        bool iscubemap;
        DX::ThrowIfFailed(
            CreateDDSTextureFromFileEx(device, resourceUpload, L"cubemap.dds",
                0, D3D12_RESOURCE_FLAG_NONE, forceSRGB, false,
                m_cubemap.ReleaseAndGetAddressOf(), nullptr, &iscubemap));

        CreateShaderResourceView(device, m_cubemap.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cubemap), iscubemap);
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    m_deviceResources->WaitForGpu();

    uploadResourcesFinished.wait();

    // Set textures
    m_vboEnvMap->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex), m_states->AnisotropicWrap());
    m_vboEnvMap->SetEnvironmentMap(m_resourceDescriptors->GetGpuHandle(Descriptors::Cubemap), m_states->AnisotropicWrap());
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { 0, 0, 6 };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    m_view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 15);
    float fogstart = -5;
    float fogend = -8;
#else
    m_view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 15);
    float fogstart = 5;
    float fogend = 8;
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    XMMATRIX orient = XMLoadFloat4x4(&m_deviceResources->GetOrientationTransform3D());
    m_projection *= orient;
#endif

#ifdef GAMMA_CORRECT_RENDERING
    XMVECTORF32 fogColor;
    fogColor.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
#else
    XMVECTOR fogColor = Colors::CornflowerBlue;
#endif

    for (auto& it : m_cupFog)
    {
        auto fog = dynamic_cast<IEffectFog*>(it.get());
        if (fog)
        {
            fog->SetFogStart(fogstart);
            fog->SetFogEnd(fogend);
            fog->SetFogColor(fogColor);
        }
    }

    m_vboNormal->SetView(m_view);
    m_vboEnvMap->SetView(m_view);

    m_vboNormal->SetProjection(m_projection);
    m_vboEnvMap->SetProjection(m_projection);
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_cup.reset();
    m_cupMesh.reset();
    m_vbo.reset();
    m_tiny.reset();
    m_soldier.reset();
    m_dwarf.reset();
    m_lmap.reset();

    m_cupNormal.clear();
    m_cupCustom.clear();
    m_cupWireframe.clear();
    m_cupFog.clear();
    m_cupVertexLighting.clear();

    m_cupMeshNormal.clear();

    m_vboNormal.reset();
    m_vboEnvMap.reset();

    m_tinyNormal.clear();

    m_soldierNormal.clear();

    m_dwarfNormal.clear();

    m_lmapNormal.clear();

    m_defaultTex.Reset();
    m_cubemap.Reset();

    m_modelResources.reset();
    m_fxFactory.reset();
    m_resourceDescriptors.reset();
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
