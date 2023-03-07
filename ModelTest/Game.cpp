//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Model
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

#define GAMMA_CORRECT_RENDERING
#define AUTOGENMIPS
#define PERPIXELLIGHTING
#define NORMALMAPS
#define USE_COPY_QUEUE
#define USE_COMPUTE_QUEUE

// Build for LH vs. RH coords
#define LH_COORDS

std::unique_ptr<Model> CreateModelFromOBJ(
    _In_opt_ ID3D12Device* device,
    _In_z_ const wchar_t* szFileName,
    bool enableInstacing,
    ModelLoaderFlags flags = ModelLoader_Default);

namespace
{
    constexpr float row0 = 2.f;
    constexpr float row1 = 0.f;
    constexpr float row2 = -2.f;

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
}

//--------------------------------------------------------------------------------------

static_assert(std::is_nothrow_move_constructible<Model>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<Model>::value, "Move Assign.");

// VS 2017 and the XDK isn't noexcept correct here
static_assert(std::is_move_constructible<ModelMesh>::value, "Move Ctor.");
static_assert(std::is_move_assignable<ModelMesh>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ModelMeshPart>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ModelMeshPart>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ModelBone>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ModelBone>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ModelBone::TransformArray>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ModelBone::TransformArray>::value, "Move Assign.");

//--------------------------------------------------------------------------------------

Game::Game() noexcept(false) :
    m_instanceCount(0),
    m_spinning(true),
    m_firstFrame(false),
    m_pitch(0),
    m_yaw(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
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

    m_bones = ModelBone::MakeArray(IEffectSkinning::MaxBones);
    XMMATRIX id = XMMatrixIdentity();
    for (size_t j = 0; j < IEffectSkinning::MaxBones; ++j)
    {
        m_bones[j] = id;
    }
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
    XMVECTOR quat;

    if (m_spinning)
    {
        world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
        quat = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
    }
    else
    {
        world = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0);
        quat = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, roll);
    }

    // Skinning settings
    float s = 1 + sin(time * 1.7f) * 0.5f;

    XMMATRIX scale = XMMatrixScaling(s, s, s);

    for (size_t j = 0; j < IEffectSkinning::MaxBones; ++j)
    {
        m_bones[j] = scale;
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
        m_cupMesh->Transition(commandList,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
        #endif

        // Compute queue IBs are in D3D12_RESOURCE_STATE_COPY_DEST state
        #ifdef USE_COMPUTE_QUEUE
        m_vbo->Transition(commandList,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
        #endif

        m_firstFrame = false;
    }

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

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

        // Custom drawing
    local = XMMatrixRotationX(cos(time)) * XMMatrixTranslation(-5.f, row0, cos(time) * 2.f);
    for (const auto& mit : m_cup->meshes)
    {
        auto mesh = mit.get();
        assert(mesh != nullptr);

        for (const auto& it : mesh->opaqueMeshParts)
        {
            auto part = it.get();
            assert(part != nullptr);

            auto effect = m_cupNormal[part->partIndex].get();

            auto imatrices = dynamic_cast<IEffectMatrices*>(effect);
            if (imatrices) imatrices->SetWorld(local);

            effect->Apply(commandList);
            part->Draw(commandList);
        }

        // Skipping alphaMeshParts for this model since we know it's empty...
        assert(mesh->alphaMeshParts.empty());
    }

        // Custom drawing using instancing
    local = XMMatrixTranslation(6.f, 0, 0);
    {
        size_t j = 0;
        for (float y = -4.f; y <= 4.f; y += 1.f)
        {
            XMMATRIX m = world * XMMatrixTranslation(0.f, y, cos(time + float(j) * XM_PIDIV4));
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

        for (const auto& mit : m_cupInst->meshes)
        {
            auto mesh = mit.get();
            assert(mesh != nullptr);

            for (const auto& it : mesh->opaqueMeshParts)
            {
                auto part = it.get();
                assert(part != nullptr);

                auto effect = m_cupInstNormal[part->partIndex].get();

                auto imatrices = dynamic_cast<IEffectMatrices*>(effect);
                if (imatrices) imatrices->SetMatrices(local, m_view, m_projection);

                effect->Apply(commandList);
                part->DrawInstanced(commandList, m_instanceCount);
            }

            // Skipping alphaMeshParts for this model since we know it's empty...
            assert(mesh->alphaMeshParts.empty());
        }
    }

    //--- Draw VBO models ------------------------------------------------------------------
    local = XMMatrixMultiply(XMMatrixScaling(0.25f, 0.25f, 0.25f), XMMatrixTranslation(4.5f, row0, 0.f));
    local = XMMatrixMultiply(world, local);
    m_vboNormal->SetWorld(local);
    m_vbo->Draw(commandList, m_vboNormal.get());

    local = XMMatrixMultiply(XMMatrixScaling(0.25f, 0.25f, 0.25f), XMMatrixTranslation(4.5f, row2, 0.f));
    local = XMMatrixMultiply(world, local);
    m_vboEnvMap->SetWorld(local);
    m_vbo->Draw(commandList, m_vboEnvMap.get());

    //--- Draw CMO models ------------------------------------------------------------------
    for (auto& it : m_teapotNormal)
    {
        auto skinnedEffect = dynamic_cast<IEffectSkinning*>(it.get());
        if (skinnedEffect)
            skinnedEffect->ResetBoneTransforms();
    }
    local = XMMatrixMultiply(XMMatrixScaling(0.01f, 0.01f, 0.01f), XMMatrixTranslation(-2.f, row1, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_teapotNormal, local, m_view, m_projection);
    m_teapot->Draw(commandList, m_teapotNormal.cbegin());

    for (auto& it : m_teapotNormal)
    {
        auto skinnedEffect = dynamic_cast<IEffectSkinning*>(it.get());
        if (skinnedEffect)
            skinnedEffect->SetBoneTransforms(m_bones.get(), IEffectSkinning::MaxBones);
    }
    local = XMMatrixMultiply(XMMatrixScaling(0.01f, 0.01f, 0.01f), XMMatrixTranslation(-3.5f, row1, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_teapotNormal, local, m_view, m_projection);
    m_teapot->Draw(commandList, m_teapotNormal.cbegin());

    local = XMMatrixMultiply(XMMatrixScaling(0.1f, 0.1f, 0.1f), XMMatrixTranslation(0.f, row1, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_gamelevelNormal, local, m_view, m_projection);
    m_gamelevel->Draw(commandList, m_gamelevelNormal.cbegin());

    local = XMMatrixMultiply(XMMatrixScaling(.2f, .2f, .2f), XMMatrixTranslation(0.f, row2, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_shipNormal, local, m_view, m_projection);
    m_ship->Draw(commandList, m_shipNormal.cbegin());

    //--- Draw SDKMESH models --------------------------------------------------------------
    local = XMMatrixTranslation(-1.f, row2, 0.f);
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

    local = XMMatrixMultiply(XMMatrixScaling(0.05f, 0.05f, 0.05f), XMMatrixTranslation(-5.0f, row1, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_nmapNormal, local, m_view, m_projection);
    m_nmap->Draw(commandList, m_nmapNormal.cbegin());

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
            skin->SetBoneTransforms(m_bones.get(), IEffectSkinning::MaxBones);
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

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

#ifdef GAMMA_CORRECT_RENDERING
    m_cup = CreateModelFromOBJ(device, L"cup._obj", false, ModelLoader_MaterialColorsSRGB);
    m_cupInst = CreateModelFromOBJ(device, L"cup._obj", true, ModelLoader_MaterialColorsSRGB);
#else
    m_cup = CreateModelFromOBJ(device, L"cup._obj", false);
    m_cupInst = CreateModelFromOBJ(device, L"cup._obj", true);
#endif

    m_vbo = Model::CreateFromVBO(device, L"player_ship_a.vbo");

    // Load textures & effects
    m_resourceDescriptors = std::make_unique<DescriptorPile>(device,
        128,
        StaticDescriptors::Reserve);

    {
        ResourceUploadBatch resourceUpload(device);

        resourceUpload.Begin();

        m_abstractModelResources = std::make_unique<EffectTextureFactory>(device, resourceUpload, m_resourceDescriptors->Heap());
        m_modelResources = std::make_unique<EffectTextureFactory>(device, resourceUpload, m_resourceDescriptors->Heap());

#ifdef GAMMA_CORRECT_RENDERING
        m_modelResources->EnableForceSRGB(true);
#endif
#ifdef AUTOGENMIPS
        m_modelResources->EnableAutoGenMips(true);
#endif

        m_abstractFXFactory = std::make_unique<EffectFactory>(m_resourceDescriptors->Heap(), m_states->Heap());
        m_fxFactory = std::make_unique<EffectFactory>(m_resourceDescriptors->Heap(), m_states->Heap());

#ifndef PERPIXELLIGHTING
        m_fxFactory->EnablePerPixelLighting(false);
#endif
#ifndef NORMALMAPS
        m_fxFactory->EnableNormalMapEffect(false);
#endif

        // Create cup materials & effects
        int txtOffset = 0;
        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_cup->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_cup->LoadTextures(*m_modelResources, txtOffset);

#ifdef LH_COORDS
        auto const& ncull = CommonStates::CullCounterClockwise;
        auto const& cull = CommonStates::CullClockwise;
#else
        auto const& ncull = CommonStates::CullClockwise;
        auto const& cull = CommonStates::CullCounterClockwise;
#endif

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            const EffectPipelineStateDescription wireframe(
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
                    basic->SetTexture(m_resourceDescriptors->GetGpuHandle(StaticDescriptors::DefaultTex), m_states->AnisotropicWrap());
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

        // Create instanced cup.
        {
            {
                size_t start, end;
                m_resourceDescriptors->AllocateRange(m_cupInst->textureNames.size(), start, end);
                txtOffset = static_cast<int>(start);
            }
            m_cupInst->LoadTextures(*m_modelResources, txtOffset);

            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            m_fxFactory->EnableInstancing(true);

            m_cupInstNormal = m_cupInst->CreateEffects(*m_fxFactory, pd, pd, txtOffset);

            m_fxFactory->EnableInstancing(false);
        }

        // Create VBO effects (no textures)

        {
            const EffectPipelineStateDescription pd(
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
        m_cupMesh = Model::CreateFromSDKMESH(device, L"cup.sdkmesh");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_cupMesh->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_cupMesh->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            m_cupMeshNormal = m_cupMesh->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        // SDKMESH Tiny
        m_tiny = Model::CreateFromSDKMESH(device, L"tiny.sdkmesh");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_tiny->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_tiny->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            m_tinyNormal = m_tiny->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        // SDKMESH Soldier
        m_soldier = Model::CreateFromSDKMESH(device, L"soldier.sdkmesh");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_soldier->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_soldier->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            m_soldierNormal = m_soldier->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        // SDKMESH Dwarf
        m_dwarf = Model::CreateFromSDKMESH(device, L"dwarf.sdkmesh");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_dwarf->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_dwarf->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            m_dwarfNormal = m_dwarf->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        // SDKMESH Lightmap
        m_lmap = Model::CreateFromSDKMESH(device, L"SimpleLightMap.sdkmesh");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_lmap->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_lmap->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            m_lmapNormal = m_lmap->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        // SDKMESH Normalmap
        m_nmap = Model::CreateFromSDKMESH(device, L"Helmet.sdkmesh");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_nmap->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_nmap->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                ncull,
                rtState);

            m_nmapNormal = m_nmap->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        // CMO teapot.cmo
        m_teapot = Model::CreateFromCMO(device, L"teapot.cmo");

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                cull,
                rtState);

            m_teapotNormal = m_teapot->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        for (auto& it : m_teapotNormal)
        {
            auto skinnedEffect = dynamic_cast<SkinnedEffect*>(it.get());
            if (skinnedEffect)
            {
                // Skinned effect always needs a texture and this model has no texture on the skinned teapot.
                skinnedEffect->SetTexture(m_resourceDescriptors->GetGpuHandle(StaticDescriptors::DefaultTex), m_states->LinearClamp());
            }
        }

        // Visual Studio CMO
        m_gamelevel = Model::CreateFromCMO(device, L"gamelevel.cmo");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_gamelevel->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_gamelevel->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                cull,
                rtState);

            m_gamelevelNormal = m_gamelevel->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

        // CMO ship
        m_ship = Model::CreateFromCMO(device, L"25ab10e8-621a-47d4-a63d-f65a00bc1549_model.cmo");

        {
            size_t start, end;
            m_resourceDescriptors->AllocateRange(m_ship->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
        }
        m_ship->LoadTextures(*m_modelResources, txtOffset);

        {
            const EffectPipelineStateDescription pd(
                nullptr,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                cull,
                rtState);

            m_shipNormal = m_ship->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
        }

#ifdef GAMMA_CORRECT_RENDERING
        constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_FORCE_SRGB;
#else
        constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_DEFAULT;
#endif

        // Load test textures
        {
            DX::ThrowIfFailed(
                CreateDDSTextureFromFileEx(device, resourceUpload, L"default.dds",
                    0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
                    m_defaultTex.ReleaseAndGetAddressOf()));

            CreateShaderResourceView(device, m_defaultTex.Get(), m_resourceDescriptors->GetCpuHandle(StaticDescriptors::DefaultTex));

            bool iscubemap;
            DX::ThrowIfFailed(
                CreateDDSTextureFromFileEx(device, resourceUpload, L"cubemap.dds",
                    0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
                    m_cubemap.ReleaseAndGetAddressOf(), nullptr, &iscubemap));

            CreateShaderResourceView(device, m_cubemap.Get(), m_resourceDescriptors->GetCpuHandle(StaticDescriptors::Cubemap), iscubemap);
        }

        // Optimize some models
        assert(!m_cup->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer);
        assert(!m_cup->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer);
        m_cup->LoadStaticBuffers(device, resourceUpload);
        assert(m_cup->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer && !m_cup->meshes[0]->opaqueMeshParts[0]->vertexBuffer);
        assert(m_cup->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer && !m_cup->meshes[0]->opaqueMeshParts[0]->indexBuffer);

#ifndef USE_COPY_QUEUE
        assert(!m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer);
        assert(!m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer);
        m_cupMesh->LoadStaticBuffers(device, resourceUpload);
        assert(m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer && !m_cupMesh->meshes[0]->opaqueMeshParts[0]->vertexBuffer);
        assert(m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer && !m_cupMesh->meshes[0]->opaqueMeshParts[0]->indexBuffer);
#endif

#ifndef USE_COMPUTE_QUEUE
        assert(!m_vbo->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer);
        assert(!m_vbo->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer);
        m_vbo->LoadStaticBuffers(device, resourceUpload, true);
        assert(m_vbo->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer&& m_vbo->meshes[0]->opaqueMeshParts[0]->vertexBuffer);
        assert(m_vbo->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer&& m_vbo->meshes[0]->opaqueMeshParts[0]->indexBuffer);
#endif

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

        assert(!m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer);
        assert(!m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer);
        m_cupMesh->LoadStaticBuffers(device, resourceUpload);
        assert(m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer && !m_cupMesh->meshes[0]->opaqueMeshParts[0]->vertexBuffer);
        assert(m_cupMesh->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer && !m_cupMesh->meshes[0]->opaqueMeshParts[0]->indexBuffer);

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

        assert(!m_vbo->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer);
        assert(!m_vbo->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer);
        m_vbo->LoadStaticBuffers(device, resourceUpload, true);
        assert(m_vbo->meshes[0]->opaqueMeshParts[0]->staticVertexBuffer&& m_vbo->meshes[0]->opaqueMeshParts[0]->vertexBuffer);
        assert(m_vbo->meshes[0]->opaqueMeshParts[0]->staticIndexBuffer&& m_vbo->meshes[0]->opaqueMeshParts[0]->indexBuffer);

        auto uploadResourcesFinished = resourceUpload.End(m_computeQueue.Get());
        uploadResourcesFinished.wait();

        m_firstFrame = true;
    }
#endif // USE_COMPUTE_QUEUE

    m_deviceResources->WaitForGpu();

    // Set textures
    m_vboEnvMap->SetTexture(m_resourceDescriptors->GetGpuHandle(StaticDescriptors::DefaultTex), m_states->AnisotropicWrap());
    m_vboEnvMap->SetEnvironmentMap(m_resourceDescriptors->GetGpuHandle(StaticDescriptors::Cubemap), m_states->AnisotropicWrap());

    // Create instance transforms.
    {
        size_t j = 0;
        for (float y = -4.f; y <= 4.f; y += 1.f)
        {
            ++j;
        }
        m_instanceCount = static_cast<UINT>(j);

        m_instanceTransforms = std::make_unique<XMFLOAT3X4[]>(j);

        constexpr XMFLOAT3X4 s_identity = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f };

        j = 0;
        for (float y = -4.f; y <= 4.f; y += 1.f)
        {
            m_instanceTransforms[j] = s_identity;
            m_instanceTransforms[j]._24 = y;
            ++j;
        }
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 6.f, 0.f } } };

    auto const size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    m_view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 15);
    constexpr float fogstart = -5;
    constexpr float fogend = -8;
#else
    m_view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 15);
    constexpr float fogstart = 5;
    constexpr float fogend = 8;
#endif

#ifdef UWP
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        m_projection *= orient;
    }
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

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_cup.reset();
    m_cupInst.reset();
    m_cupMesh.reset();
    m_vbo.reset();
    m_tiny.reset();
    m_soldier.reset();
    m_dwarf.reset();
    m_lmap.reset();
    m_nmap.reset();
    m_teapot.reset();
    m_gamelevel.reset();
    m_ship.reset();

    m_cupNormal.clear();
    m_cupCustom.clear();
    m_cupWireframe.clear();
    m_cupFog.clear();
    m_cupVertexLighting.clear();

    m_cupInstNormal.clear();
    m_cupMeshNormal.clear();

    m_vboNormal.reset();
    m_vboEnvMap.reset();

    m_tinyNormal.clear();
    m_soldierNormal.clear();
    m_dwarfNormal.clear();
    m_lmapNormal.clear();
    m_nmapNormal.clear();

    m_teapotNormal.clear();
    m_gamelevelNormal.clear();
    m_shipNormal.clear();

    m_defaultTex.Reset();
    m_cubemap.Reset();

    m_abstractModelResources.reset();
    m_modelResources.reset();

    m_abstractFXFactory.reset();
    m_fxFactory.reset();

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
