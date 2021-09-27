//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Model animation
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float row0 = 2.f;
    constexpr float row1 = 0.f;
    constexpr float row2 = -2.f;

    void DumpBones(const ModelBone::Collection& bones, _In_z_ const char* name)
    {
        char buff[128] = {};
        if (bones.empty())
        {
            sprintf_s(buff, "ERROR: %s is missing model bones!\n", name);
            OutputDebugStringA(buff);
        }
        else
        {
            sprintf_s(buff, "%s: contains %zu bones\n", name, bones.size());
            OutputDebugStringA(buff);

            for (auto it : bones)
            {
                sprintf_s(buff, "\t'%ls' (%s | %s | %s)\n",
                    it.name.c_str(),
                    (it.childIndex != ModelBone::c_Invalid) ? "has children" : "no children",
                    (it.siblingIndex != ModelBone::c_Invalid) ? "has sibling" : "no siblings",
                    (it.parentIndex != ModelBone::c_Invalid) ? "has parent" : "no parent");
                OutputDebugStringA(buff);
            }
        }
    }

    void DumpMatrices(
        size_t nbones,
        _In_reads_(nbones) const XMMATRIX* boneTransforms,
        _In_z_ const char* name)
    {
        char buff[128] = {};
        if (!nbones || !boneTransforms)
        {
            sprintf_s(buff, "ERROR: %s is missing bone transforms!\n", name);
            OutputDebugStringA(buff);
        }
        else
        {
            sprintf_s(buff, "%s: transforms for %zu bones\n", name, nbones);
            OutputDebugStringA(buff);

            for (size_t j = 0; j < nbones; ++j)
            {
                auto m = reinterpret_cast<const XMFLOAT4X4*>(&boneTransforms[j]);
                sprintf_s(buff, "\t[%zu]\t[%f %f %f %f]\n",
                    j, m->_11, m->_12, m->_13, m->_14);
                OutputDebugStringA(buff);

                sprintf_s(buff, "\t\t[%f %f %f %f]\n",
                    m->_21, m->_22, m->_23, m->_24);
                OutputDebugStringA(buff);

                sprintf_s(buff, "\t\t[%f %f %f %f]\n",
                    m->_31, m->_32, m->_33, m->_34);
                OutputDebugStringA(buff);

                sprintf_s(buff, "\t\t[%f %f %f %f]\n",
                    m->_41, m->_42, m->_43, m->_44);
                OutputDebugStringA(buff);
            }
        }
    }
}

// Constructor.
Game::Game() noexcept(false)
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
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
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

    auto time = static_cast<float>(m_timer.GetTotalSeconds());

    XMMATRIX world = XMMatrixRotationY(XM_PI);

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
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    // Tank scene (rigid-body animation)
    XMMATRIX local = XMMatrixMultiply(XMMatrixScaling(0.3f, 0.3f, 0.3f), XMMatrixTranslation(0.f, row2, 0.f));
    local = XMMatrixMultiply(world, local);

    constexpr uint32_t rootBone = 0;
    uint32_t tankBone = ModelBone::c_Invalid;
    uint32_t barricadeBone = ModelBone::c_Invalid;
    uint32_t nbones = 0;
    {
        for (auto it : m_tank->bones)
        {
            if (_wcsicmp(L"tank_geo", it.name.c_str()) == 0)
            {
                tankBone = nbones;
            }
            else if (_wcsicmp(L"barricade_geo", it.name.c_str()) == 0)
            {
                barricadeBone = nbones;
            }

            ++nbones;
        }

        assert(nbones == m_tank->bones.size());
    }

    m_tank->boneMatrices[rootBone] = XMMatrixRotationY((time / 10.f) * XM_PI);
    m_tank->boneMatrices[tankBone] = XMMatrixRotationY(time * XM_PI);

    m_tank->boneMatrices[barricadeBone] = XMMatrixTranslation(0.f, cosf(time) * 2.f, 0.f);

    auto bones = ModelBone::MakeArray(nbones);
    m_tank->CopyAbsoluteBoneTransformsTo(nbones, bones.get());
        // For SDKMESH rigid-body, the matrix data is the local position to use.

    Model::UpdateEffectMatrices(m_tankNormal, local, m_view, m_projection);
    m_tank->Draw(commandList, nbones, bones.get(), local, m_tankNormal.cbegin());

    // Teapot (direct-mapped bones)
    // TODO - Needs CMO loading

    // Draw SDKMESH models (bone influences)
    uint32_t upperArmR = ModelBone::c_Invalid;
    uint32_t upperArmL = ModelBone::c_Invalid;
    uint32_t leftLeg = ModelBone::c_Invalid;
    uint32_t rightLeg = ModelBone::c_Invalid;
    nbones = 0;
    {
        for (auto it : m_soldier->bones)
        {
            if (_wcsicmp(L"R_UpperArm", it.name.c_str()) == 0)
            {
                upperArmR = nbones;
            }
            else if (_wcsicmp(L"L_UpperArm", it.name.c_str()) == 0)
            {
                upperArmL = nbones;
            }
            else if (_wcsicmp(L"L_Thigh1", it.name.c_str()) == 0)
            {
                leftLeg = nbones;
            }
            else if (_wcsicmp(L"R_Thigh", it.name.c_str()) == 0)
            {
                rightLeg = nbones;
            }

            ++nbones;
        }

        assert(nbones == m_soldier->bones.size());
    }

    for(auto it : m_soldierNormal)
    {
        auto skinnedEffect = dynamic_cast<IEffectSkinning*>(it.get());
        if (skinnedEffect)
        {
            skinnedEffect->ResetBoneTransforms();
        }
    }
    local = XMMatrixMultiply(XMMatrixScaling(2.f, 2.f, 2.f), XMMatrixTranslation(2.f, row0, 0.f));
    local = XMMatrixMultiply(world, local);
    Model::UpdateEffectMatrices(m_soldierNormal, local, m_view, m_projection);
    m_soldier->Draw(commandList, m_soldierNormal.cbegin());

    local = XMMatrixMultiply(XMMatrixScaling(2.f, 2.f, 2.f), XMMatrixTranslation(2.f, row1, 0.f));
    local = XMMatrixMultiply(world, local);

    auto animBones = ModelBone::MakeArray(nbones);
    m_soldier->CopyBoneTransformsTo(nbones, animBones.get());
    animBones[upperArmL] = XMMatrixRotationX(time * XM_PI);
    animBones[upperArmR] = XMMatrixRotationX(time * XM_PI);
    animBones[rightLeg] = XMMatrixMultiply(XMMatrixRotationX(-time * XM_PI), XMMatrixRotationZ(XM_PIDIV2 + XM_PIDIV4));
    animBones[leftLeg] = XMMatrixMultiply(XMMatrixRotationX(time * XM_PI), XMMatrixRotationZ(XM_PI));

    auto targetBones = ModelBone::MakeArray(nbones);
    m_soldier->CopyAbsoluteBoneTransforms(nbones, animBones.get(), targetBones.get());

    bones = ModelBone::MakeArray(nbones);

    for (size_t j = 0; j < nbones; ++j)
    {
        bones[j] = XMMatrixMultiply(m_soldier->invBindPoseMatrices[j], targetBones[j]);
    }

    m_soldier->DrawSkinned(commandList, nbones, bones.get(), local, m_soldierNormal.cbegin());

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

    m_states = std::make_unique<CommonStates>(device);

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    // DirectX SDK Mesh
    ModelLoaderFlags flags = ModelLoader_IncludeBones;

    m_tank = Model::CreateFromSDKMESH(device, L"TankScene.sdkmesh", flags);

    DumpBones(m_tank->bones, "TankScene.sdkmesh");

    m_soldier = Model::CreateFromSDKMESH(device, L"soldier.sdkmesh", flags);

    DumpBones(m_soldier->bones, "soldier.sdkmesh");

    // Load textures & effects
    m_resourceDescriptors = std::make_unique<DescriptorPile>(device, 128);

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    m_tank->LoadStaticBuffers(device, resourceUpload);
    m_soldier->LoadStaticBuffers(device, resourceUpload);

    m_modelResources = std::make_unique<EffectTextureFactory>(device, resourceUpload, m_resourceDescriptors->Heap());

#ifdef GAMMA_CORRECT_RENDERING
    m_modelResources->EnableForceSRGB(true);
#endif

    m_fxFactory = std::make_unique<EffectFactory>(m_resourceDescriptors->Heap(), m_states->Heap());

    // Create tank materials & effects
    int txtOffset = 0;
    {
        size_t start, end;
        m_resourceDescriptors->AllocateRange(m_tank->textureNames.size(), start, end);
        txtOffset = static_cast<int>(start);
    }
    m_tank->LoadTextures(*m_modelResources, txtOffset);

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

        m_tankNormal = m_tank->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
    }

    {
        size_t start, end;
        m_resourceDescriptors->AllocateRange(m_soldier->textureNames.size(), start, end);
        txtOffset = static_cast<int>(start);
    }
    m_soldier->LoadTextures(*m_modelResources, txtOffset);

    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            ncull,
            rtState);

        m_soldierNormal = m_soldier->CreateEffects(*m_fxFactory, pd, pd, txtOffset);
    }

    // TODO - Load teapot from CMO

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 6.f, 0.f } } };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    m_view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 15);
#else
    m_view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    m_projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 15);
#endif

#ifdef UWP
    XMMATRIX orient = XMLoadFloat4x4(&m_deviceResources->GetOrientationTransform3D());
    m_projection *= orient;
#endif
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_soldier.reset();
    m_soldierNormal.clear();

    m_tank.reset();
    m_tankNormal.clear();

    m_states.reset();
    m_fxFactory.reset();
    m_modelResources.reset();
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
