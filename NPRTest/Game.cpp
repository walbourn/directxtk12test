//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectX Tool Kit - NPREffect
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"
#include "Bezier.h"
#include "FindMedia.h"

#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float row0 = 3.0f;
    constexpr float row1 = 1.5f;
    constexpr float row2 = 0.f;
    constexpr float row3 = -1.5f;
    constexpr float row4 = -3.0f;

    constexpr float col0 = -6.f;
    constexpr float col1 = -4.f;
    constexpr float col2 = -2.f;
    constexpr float col3 = 0.f;
    constexpr float col4 = 2.f;
    constexpr float col5 = 4.f;
    constexpr float col6 = 6.f;

    struct TestVertex
    {
        TestVertex(FXMVECTOR iposition, FXMVECTOR inormal, FXMVECTOR itextureCoordinate)
        {
            XMStoreFloat3(&this->position, iposition);
            XMStoreFloat3(&this->normal, inormal);
            XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);
            XMStoreUByte4(&this->blendIndices, XMVectorSet(0, 1, 2, 3));

            float u = XMVectorGetX(itextureCoordinate) - 0.5f;
            float v = XMVectorGetY(itextureCoordinate) - 0.5f;

            float d = 1 - sqrtf(u * u + v * v) * 2;

            if (d < 0)
                d = 0;

            XMStoreFloat4(&this->blendWeight, XMVectorSet(d, 1 - d, u, v));

            color = 0xFFFF00FF;
        }

        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT2 textureCoordinate;
        XMUBYTE4 blendIndices;
        XMFLOAT4 blendWeight;
        XMUBYTE4 color;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;

    private:
        static constexpr unsigned int InputElementCount = 6;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };

    const D3D12_INPUT_ELEMENT_DESC TestVertex::InputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    const D3D12_INPUT_LAYOUT_DESC TestVertex::InputLayout =
    {
        TestVertex::InputElements,
        TestVertex::InputElementCount
    };

    using VertexCollection = std::vector<TestVertex>;
    using IndexCollection = std::vector<uint16_t>;

    #include "../../Src/TeapotData.inc"

    // Tessellates the specified bezier patch.
    void TessellatePatch(VertexCollection& vertices, IndexCollection& indices, TeapotPatch const& patch, FXMVECTOR scale, bool isMirrored)
    {
        constexpr int tessellation = 16;

        // Look up the 16 control points for this patch.
        XMVECTOR controlPoints[16];

        for (int i = 0; i < 16; i++)
        {
            controlPoints[i] = XMVectorMultiply(TeapotControlPoints[patch.indices[i]], scale);
        }

        // Create the index data.
        Bezier::CreatePatchIndices(tessellation, isMirrored, [&](size_t index)
        {
            indices.push_back((uint16_t)(vertices.size() + index));
        });

        // Create the vertex data.
        Bezier::CreatePatchVertices(controlPoints, tessellation, isMirrored, [&](FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate)
        {
            vertices.push_back(TestVertex(position, normal, textureCoordinate));
        });
    }

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
    const XMVECTORF32 c_rimToonColor = { { { 0.6f, 0.8f, 1.0f } } };
    const XMVECTORF32 c_rimGoochColor = { { { 1.f, 1.f, 1.f, 1.f, } } };

    static const wchar_t* s_searchFolders[] =
    {
        L"NPRTest",
        L"Tests\\NPRTest",
        nullptr
    };
} // anonymous namespace


//--------------------------------------------------------------------------------------

Game::Game() noexcept(false) :
    m_indexCount(0),
    m_vertexBufferView{},
    m_indexBufferView{},
    m_frame(0)
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
void Game::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

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
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    // Time-based animation
    float time = static_cast<float>(m_timer.GetTotalSeconds());

    float alphaFade = (sin(time * 2) + 1) / 2;

    if (alphaFade >= 1)
        alphaFade = 1 - FLT_EPSILON;

    float yaw = time * 0.4f;
    float pitch = time * 0.7f;
    float roll = time * 1.1f;

    XMMATRIX world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

    // Setup for teapot drawing.
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //--- NPREffect: Cel shading -----------------------------------------------------------

    // Default cel shading (4 bands).
    m_celEffect->SetWorld(world * XMMatrixTranslation(col0, row0, 0));
    m_celEffect->SetCelShaderBands(4);
    m_celEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with 2 bands.
    m_celEffect->SetWorld(world * XMMatrixTranslation(col1, row0, 0));
    m_celEffect->SetCelShaderBands(2);
    m_celEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with 8 bands.
    m_celEffect->SetWorld(world * XMMatrixTranslation(col2, row0, 0));
    m_celEffect->SetCelShaderBands(8);
    m_celEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading, no specular.
    m_celEffectNoSpecular->SetWorld(world * XMMatrixTranslation(col3, row0, 0));
    m_celEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading, no rim lighting.
    m_celEffectNoRim->SetWorld(world * XMMatrixTranslation(col4, row0, 0));
    m_celEffectNoRim->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with vertex color.
    m_celEffectVc->SetWorld(world * XMMatrixTranslation(col5, row0, 0));
    m_celEffectVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with 4 bands and texture.
    m_celEffectTx->SetWorld(world * XMMatrixTranslation(col0, row1, 0));
    m_celEffectTx->SetCelShaderBands(4);
    m_celEffectTx->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with 2 bands and texture.
    m_celEffectTx->SetWorld(world * XMMatrixTranslation(col1, row1, 0));
    m_celEffectTx->SetCelShaderBands(2);
    m_celEffectTx->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with 8 bands and texture.
    m_celEffectTx->SetWorld(world * XMMatrixTranslation(col2, row1, 0));
    m_celEffectTx->SetCelShaderBands(8);
    m_celEffectTx->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with texture, no specular.
    m_celEffectTxNoSpecular->SetWorld(world * XMMatrixTranslation(col3, row1, 0));
    m_celEffectTxNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with texture, no rim lighting.
    m_celEffectTxNoRim->SetWorld(world * XMMatrixTranslation(col4, row1, 0));
    m_celEffectTxNoRim->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading with vertex color and texture.
    m_celEffectTxVc->SetWorld(world * XMMatrixTranslation(col5, row1, 0));
    m_celEffectTxVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Cel shading (4 bands) with alpha.
    m_celEffect->SetWorld(world * XMMatrixTranslation(col1, row3, 0));
    m_celEffect->SetCelShaderBands(4);
    m_celEffect->SetAlpha(alphaFade);
    m_celEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    m_celEffect->SetAlpha(1.f);

    //--- NPREffect: Gooch shading ---------------------------------------------------------

    // Default Gooch shading.
    m_goochEffect->SetWorld(world * XMMatrixTranslation(col0, row2, 0));
    m_goochEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with custom cool/warm colors.
    m_goochEffectCustom->SetGoochCoolColor(Colors::Red, 0.4f);
    m_goochEffectCustom->SetGoochWarmColor(Colors::Green, 0.4f);
    m_goochEffectCustom->SetWorld(world * XMMatrixTranslation(col1, row2, 0));
    m_goochEffectCustom->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_goochEffectCustom->SetGoochCoolColor(Colors::Black, 0.1f);
    m_goochEffectCustom->SetGoochWarmColor(Colors::Blue, 0.1f);
    m_goochEffectCustom->SetWorld(world * XMMatrixTranslation(col2, row2, 0));
    m_goochEffectCustom->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading, no specular.
    m_goochEffectNoSpecular->SetWorld(world * XMMatrixTranslation(col3, row2, 0));
    m_goochEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading, no rim lighting.
    m_goochEffectNoRim->SetWorld(world * XMMatrixTranslation(col4, row2, 0));
    m_goochEffectNoRim->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with vertex color.
    m_goochEffectVc->SetWorld(world * XMMatrixTranslation(col5, row2, 0));
    m_goochEffectVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with texture.
    m_goochEffectTx->SetWorld(world * XMMatrixTranslation(col0, row3, 0));
    m_goochEffectTx->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with texture, no specular.
    m_goochEffectTxNoSpecular->SetWorld(world * XMMatrixTranslation(col3, row3, 0));
    m_goochEffectTxNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with texture, no rim lighting.
    m_goochEffectTxNoRim->SetWorld(world * XMMatrixTranslation(col4, row3, 0));
    m_goochEffectTxNoRim->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Gooch shading with vertex color.
    m_goochEffectTxVc->SetWorld(world * XMMatrixTranslation(col5, row3, 0));
    m_goochEffectTxVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Default Gooch shading with alpha.
    m_goochEffect->SetAlpha(alphaFade);
    m_goochEffect->SetWorld(world * XMMatrixTranslation(col2, row3, 0));
    m_goochEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    m_goochEffect->SetAlpha(1.f);

    //--- NPREffect: MatCap shading --------------------------------------------------------

    // Default MatCap shading
    auto matcap1 = m_resourceDescriptors->GetGpuHandle(Descriptors::MatCap1);
    m_matcapEffect->SetWorld(world * XMMatrixTranslation(col0, row4, 0));
    m_matcapEffect->SetMatCap(matcap1);
    m_matcapEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    auto matcap2 = m_resourceDescriptors->GetGpuHandle(Descriptors::MatCap2);
    m_matcapEffect->SetAlpha(alphaFade);
    m_matcapEffect->SetWorld(world * XMMatrixTranslation(col1, row4, 0));
    m_matcapEffect->SetMatCap(matcap2);
    m_matcapEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    m_matcapEffect->SetAlpha(1.f);

    // MatCap shading with vertex color.
    m_matcapEffectVc->SetWorld(world * XMMatrixTranslation(col2, row4, 0));
    m_matcapEffectVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Matcap shading with texture.
    m_matcapEffectTx->SetWorld(world * XMMatrixTranslation(col3, row4, 0));
    m_matcapEffectTx->SetMatCap(matcap1);
    m_matcapEffectTx->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_matcapEffectTx->SetAlpha(alphaFade);
    m_matcapEffectTx->SetWorld(world * XMMatrixTranslation(col4, row4, 0));
    m_matcapEffectTx->SetMatCap(matcap2);
    m_matcapEffectTx->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    m_matcapEffectTx->SetAlpha(1.f);

    // Matcap sahding with vertex color and texture.
    m_matcapEffectTxVc->SetWorld(world * XMMatrixTranslation(col5, row4, 0));
    m_matcapEffectTxVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- SkinnedNPREffect -----------------------------------------------------------------
    XMMATRIX bones[4] =
    {
        XMMatrixIdentity(),
        XMMatrixIdentity(),
        XMMatrixScaling(0, 0, 0),
        XMMatrixScaling(0, 0, 0),
    };

    m_skinnedCelEffect->SetBoneTransforms(bones, std::size(bones));
    m_skinnedCelEffect->SetWorld(world * XMMatrixTranslation(col6, row0, 0));
    m_skinnedCelEffect->SetCelShaderBands(4);
    m_skinnedCelEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_skinnedGoochEffect->SetBoneTransforms(bones, std::size(bones));
    m_skinnedGoochEffect->SetWorld(world * XMMatrixTranslation(col6, row1, 0));
    m_skinnedGoochEffect->SetCelShaderBands(4);
    m_skinnedGoochEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_skinnedMatcapEffect->SetBoneTransforms(bones, std::size(bones));
    m_skinnedMatcapEffect->SetWorld(world * XMMatrixTranslation(col6, row2, 0));
    m_skinnedMatcapEffect->SetCelShaderBands(4);
    m_skinnedMatcapEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Skinned effect, variable scaling transforms.
    float scales[4] =
    {
        1 + sin(time * 1.7f) * 0.5f,
        1 + sin(time * 2.3f) * 0.5f,
        0,
        0,
    };

    for (int i = 0; i < 4; i++)
    {
        bones[i] = XMMatrixScaling(scales[i], scales[i], scales[i]);
    }

    m_skinnedCelEffect->SetBoneTransforms(bones, std::size(bones));
    m_skinnedCelEffect->SetWorld(world * XMMatrixTranslation(col6, row3, 0));
    m_skinnedCelEffect->SetCelShaderBands(4);
    m_skinnedCelEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Skinned effect, different variable scaling transforms.
    float scales2[4] =
    {
        1,
        1,
        sin(time * 2.3f) * 0.5f,
        sin(time * 3.1f) * 0.5f,
    };

    for (int i = 0; i < 4; i++)
    {
        bones[i] = XMMatrixScaling(scales2[i], scales2[i], scales2[i]);
    }

    m_skinnedCelEffect->SetBoneTransforms(bones, std::size(bones));
    m_skinnedCelEffect->SetWorld(world * XMMatrixTranslation(col6, row4, 0));
    m_skinnedCelEffect->SetCelShaderBands(4);
    m_skinnedCelEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

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
#pragma endregion

#pragma region Message Handlers
// Message handlers
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

    m_states = std::make_unique<CommonStates>(device);

    CreateTeapot();

    // Create NPREffect instances
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    XMVECTORF32 blue, red, green, grey;
#ifdef GAMMA_CORRECT_RENDERING
    blue.v = XMColorSRGBToRGB(Colors::Blue);
    red.v = XMColorSRGBToRGB(Colors::Red);
    green.v = XMColorSRGBToRGB(Colors::Green);
    grey.v = XMColorSRGBToRGB(Colors::Gray);
#else
    blue.v = Colors::Blue;
    red.v = Colors::Red;
    green.v = Colors::Green;
    grey.v = Colors::Gray;
#endif

    {
        const EffectPipelineStateDescription pdOpaque(
            &TestVertex::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
#ifdef LH_COORDS
            CommonStates::CullClockwise,
#else
            CommonStates::CullCounterClockwise,
#endif
            rtState);

        const EffectPipelineStateDescription pdAlpha(
            &TestVertex::InputLayout,
            CommonStates::AlphaBlend,
            CommonStates::DepthDefault,
        #ifdef LH_COORDS
            CommonStates::CullClockwise,
        #else
            CommonStates::CullCounterClockwise,
        #endif
            rtState);

        //--- Cel shading (Mode_Cel) -------------------------------------------------------

        // Default cel shading.
        m_celEffect = std::make_unique<NPREffect>(device, EffectFlags::None, pdAlpha, NPREffect::Mode_Cel);
        m_celEffect->EnableDefaultLighting();
        m_celEffect->SetDiffuseColor(blue);
        m_celEffect->SetRimLightingColor(c_rimToonColor);
        m_celEffect->SetCelShaderBands(4);

        // Cel shading, no specular.
        m_celEffectNoSpecular = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectNoSpecular->EnableDefaultLighting();
        m_celEffectNoSpecular->SetDiffuseColor(blue);
        m_celEffectNoSpecular->SetRimLightingColor(c_rimToonColor);
        m_celEffectNoSpecular->SetCelShaderBands(4);
        m_celEffectNoSpecular->DisableSpecular();

        // Cel shading, no rim lighting.
        m_celEffectNoRim = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectNoRim->EnableDefaultLighting();
        m_celEffectNoRim->SetDiffuseColor(blue);
        m_celEffectNoRim->SetCelShaderBands(4);
        m_celEffectNoRim->DisableRimLighting();

        // Cel shading with vertex color.
        m_celEffectVc = std::make_unique<NPREffect>(device, EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectVc->EnableDefaultLighting();
        m_celEffectVc->SetRimLightingColor(c_rimToonColor);
        m_celEffectVc->SetCelShaderBands(4);

        // Cel shading with texture.
        m_celEffectTx = std::make_unique<NPREffect>(device, EffectFlags::Texture, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectTx->EnableDefaultLighting();
        m_celEffectTx->SetRimLightingColor(c_rimToonColor);
        m_celEffectTx->SetCelShaderBands(4);

        // Cel shading with texture, no specular
        m_celEffectTxNoSpecular = std::make_unique<NPREffect>(device, EffectFlags::Texture, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectTxNoSpecular->EnableDefaultLighting();
        m_celEffectTxNoSpecular->SetRimLightingColor(c_rimToonColor);
        m_celEffectTxNoSpecular->SetCelShaderBands(4);
        m_celEffectTxNoSpecular->DisableSpecular();

        // Cel shading with texture, no rim lighting.
        m_celEffectTxNoRim = std::make_unique<NPREffect>(device, EffectFlags::Texture, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectTxNoRim->EnableDefaultLighting();
        m_celEffectTxNoRim->SetCelShaderBands(4);
        m_celEffectTxNoRim->DisableRimLighting();

        // Cel shading with vertex color and texture.
        m_celEffectTxVc = std::make_unique<NPREffect>(device, EffectFlags::Texture | EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_Cel);
        m_celEffectTxVc->EnableDefaultLighting();
        m_celEffectTxVc->SetRimLightingColor(c_rimToonColor);
        m_celEffectTxVc->SetCelShaderBands(4);

        // Cel shading with skinning.
        m_skinnedCelEffect = std::make_unique<SkinnedNPREffect>(device, EffectFlags::Texture, pdOpaque,
            SkinnedNPREffect::Mode_Cel);
        m_skinnedCelEffect->EnableDefaultLighting();
        m_skinnedCelEffect->SetRimLightingColor(c_rimToonColor);
        m_skinnedCelEffect->SetCelShaderBands(4);

        //--- Gooch shading (Mode_Gooch) ---------------------------------------------------

        // Default Gooch shading.
        m_goochEffect = std::make_unique<NPREffect>(device, EffectFlags::None, pdAlpha, NPREffect::Mode_Gooch);
        m_goochEffect->EnableDefaultLighting();
        m_goochEffect->SetDiffuseColor(grey);
        m_goochEffect->SetRimLightingColor(c_rimGoochColor);

        // Gooch shading, no specular.
        m_goochEffectNoSpecular = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectNoSpecular->EnableDefaultLighting();
        m_goochEffectNoSpecular->SetDiffuseColor(grey);
        m_goochEffectNoSpecular->SetRimLightingColor(c_rimGoochColor);
        m_goochEffectNoSpecular->DisableSpecular();

        // Gooch shading, no rim lighting.
        m_goochEffectNoRim = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectNoRim->EnableDefaultLighting();
        m_goochEffectNoRim->SetDiffuseColor(grey);
        m_goochEffectNoRim->DisableRimLighting();

        // Gooch shading with vertex color.
        m_goochEffectVc = std::make_unique<NPREffect>(device, EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectVc->EnableDefaultLighting();
        m_goochEffectVc->SetRimLightingColor(c_rimGoochColor);

        // Gooch shading with custom cool/warm colors.
        m_goochEffectCustom = std::make_unique<NPREffect>(device, EffectFlags::None, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectCustom->EnableDefaultLighting();
        m_goochEffectCustom->SetDiffuseColor(grey);
        m_goochEffectCustom->SetRimLightingColor(c_rimGoochColor);

        // Gooch shading with texture.
        m_goochEffectTx = std::make_unique<NPREffect>(device, EffectFlags::Texture, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectTx->SetDiffuseColor(grey);
        m_goochEffectTx->SetRimLightingColor(c_rimGoochColor);
        m_goochEffectTx->EnableDefaultLighting();
        m_goochEffectTx->SetGoochCoolColor(red, 0.4f);
        m_goochEffectTx->SetGoochWarmColor(green, 0.4f);

        // Gooch shading with texture, no specular.
        m_goochEffectTxNoSpecular = std::make_unique<NPREffect>(device, EffectFlags::Texture, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectTxNoSpecular->SetDiffuseColor(grey);
        m_goochEffectTxNoSpecular->SetRimLightingColor(c_rimGoochColor);
        m_goochEffectTxNoSpecular->EnableDefaultLighting();
        m_goochEffectTxNoSpecular->SetGoochCoolColor(red, 0.4f);
        m_goochEffectTxNoSpecular->SetGoochWarmColor(green, 0.4f);
        m_goochEffectTxNoSpecular->DisableSpecular();

        // Gooch shading with texture, no rim lighting.
        m_goochEffectTxNoRim = std::make_unique<NPREffect>(device, EffectFlags::Texture, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectTxNoRim->SetDiffuseColor(grey);
        m_goochEffectTxNoRim->EnableDefaultLighting();
        m_goochEffectTxNoRim->SetGoochCoolColor(red, 0.4f);
        m_goochEffectTxNoRim->SetGoochWarmColor(green, 0.4f);
        m_goochEffectTxNoRim->DisableRimLighting();

        // Gooch shading with vertex color and texture.
        m_goochEffectTxVc = std::make_unique<NPREffect>(device, EffectFlags::Texture | EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_Gooch);
        m_goochEffectTxVc->SetDiffuseColor(grey);
        m_goochEffectTxVc->EnableDefaultLighting();
        m_goochEffectTxVc->SetRimLightingColor(c_rimGoochColor);
        m_goochEffectTxVc->SetGoochCoolColor(red, 0.4f);
        m_goochEffectTxVc->SetGoochWarmColor(green, 0.4f);

        // Gooch shading with skinning.
        m_skinnedGoochEffect = std::make_unique<SkinnedNPREffect>(device, EffectFlags::Texture, pdOpaque,
            SkinnedNPREffect::Mode_Gooch);
        m_skinnedGoochEffect->SetDiffuseColor(grey);
        m_skinnedGoochEffect->SetRimLightingColor(c_rimGoochColor);
        m_skinnedGoochEffect->EnableDefaultLighting();
        m_skinnedGoochEffect->SetGoochCoolColor(red, 0.4f);
        m_skinnedGoochEffect->SetGoochWarmColor(green, 0.4f);

        //--- MatCap shading (Mode_MatCap) -------------------------------------------------

        // Default MatCap shading
        m_matcapEffect = std::make_unique<NPREffect>(device, EffectFlags::None, pdAlpha, NPREffect::Mode_MatCap);

        // MatCap shading with vertex color.
        m_matcapEffectVc = std::make_unique<NPREffect>(device, EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_MatCap);

        // Matcap shading with texture.
        m_matcapEffectTx = std::make_unique<NPREffect>(device, EffectFlags::Texture, pdAlpha, NPREffect::Mode_MatCap);

        // Matcap sahding with vertex color and texture.
        m_matcapEffectTxVc = std::make_unique<NPREffect>(device, EffectFlags::Texture | EffectFlags::VertexColor, pdOpaque, NPREffect::Mode_MatCap);

        // Matcap shading with skinning.
        m_skinnedMatcapEffect = std::make_unique<SkinnedNPREffect>(device, EffectFlags::Texture, pdOpaque,
            SkinnedNPREffect::Mode_MatCap);
    }

    // Load textures.
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

#ifdef GAMMA_CORRECT_RENDERING
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_FORCE_SRGB;
    constexpr WIC_LOADER_FLAGS wicLoadFlags = WIC_LOADER_FORCE_SRGB;
#else
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_DEFAULT;
    constexpr WIC_LOADER_FLAGS wicLoadFlags = WIC_LOADER_DEFAULT;
#endif

    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"reftexture.dds", s_searchFolders);
    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, strFilePath,
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_refTexture.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_refTexture.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::RefTexture));

    DX::FindMediaFile(strFilePath, MAX_PATH, L"matcap_gold.png", s_searchFolders);
    DX::ThrowIfFailed(
        CreateWICTextureFromFileEx(device, resourceUpload, strFilePath,
            0, D3D12_RESOURCE_FLAG_NONE, wicLoadFlags,
            m_matCapTexture1.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_matCapTexture1.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::MatCap1));

    DX::FindMediaFile(strFilePath, MAX_PATH, L"matcap_ice.png", s_searchFolders);
    DX::ThrowIfFailed(
        CreateWICTextureFromFileEx(device, resourceUpload, strFilePath,
            0, D3D12_RESOURCE_FLAG_NONE, wicLoadFlags,
            m_matCapTexture2.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_matCapTexture2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::MatCap2));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();

    // Set textures.
    const auto refTexture = m_resourceDescriptors->GetGpuHandle(Descriptors::RefTexture);
    const auto matcap = m_resourceDescriptors->GetGpuHandle(Descriptors::MatCap1);
    const auto sampler = m_states->LinearWrap();

    m_celEffectTx->SetTexture(refTexture, sampler);
    m_celEffectTxNoSpecular->SetTexture(refTexture, sampler);
    m_celEffectTxNoRim->SetTexture(refTexture, sampler);
    m_celEffectTxVc->SetTexture(refTexture, sampler);
    m_skinnedCelEffect->SetTexture(refTexture, sampler);

    m_goochEffectTx->SetTexture(refTexture, sampler);
    m_goochEffectTxNoSpecular->SetTexture(refTexture, sampler);
    m_goochEffectTxNoRim->SetTexture(refTexture, sampler);
    m_goochEffectTxVc->SetTexture(refTexture, sampler);
    m_skinnedGoochEffect->SetTexture(refTexture, sampler);

    m_matcapEffect->SetMatCap(matcap, sampler);
    m_matcapEffectVc->SetMatCap(matcap, sampler);

    m_matcapEffectTx->SetTexture(refTexture, sampler);
    m_matcapEffectTx->SetMatCap(matcap);
    m_matcapEffectTxVc->SetTexture(refTexture, sampler);
    m_matcapEffectTxVc->SetMatCap(matcap);
    m_skinnedMatcapEffect->SetTexture(refTexture, sampler);
    m_skinnedMatcapEffect->SetMatCap(matcap);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 7.f, 0.f } } };

    const auto size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 15);
#else
    XMMATRIX view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 15);
#endif

#ifdef UWP
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        projection *= orient;
    }
#endif

    m_celEffect->SetView(view);
    m_celEffectNoSpecular->SetView(view);
    m_celEffectNoRim->SetView(view);
    m_celEffectVc->SetView(view);
    m_celEffectTx->SetView(view);
    m_celEffectTxNoSpecular->SetView(view);
    m_celEffectTxNoRim->SetView(view);
    m_celEffectTxVc->SetView(view);
    m_skinnedCelEffect->SetView(view);

    m_goochEffect->SetView(view);
    m_goochEffectNoSpecular->SetView(view);
    m_goochEffectNoRim->SetView(view);
    m_goochEffectVc->SetView(view);
    m_goochEffectCustom->SetView(view);
    m_goochEffectTx->SetView(view);
    m_goochEffectTxNoSpecular->SetView(view);
    m_goochEffectTxNoRim->SetView(view);
    m_goochEffectTxVc->SetView(view);
    m_skinnedGoochEffect->SetView(view);

    m_matcapEffect->SetView(view);
    m_matcapEffectVc->SetView(view);
    m_matcapEffectTx->SetView(view);
    m_matcapEffectTxVc->SetView(view);
    m_skinnedMatcapEffect->SetView(view);

    m_celEffect->SetProjection(projection);
    m_celEffectNoSpecular->SetProjection(projection);
    m_celEffectNoRim->SetProjection(projection);
    m_celEffectVc->SetProjection(projection);
    m_celEffectTx->SetProjection(projection);
    m_celEffectTxNoSpecular->SetProjection(projection);
    m_celEffectTxNoRim->SetProjection(projection);
    m_celEffectTxVc->SetProjection(projection);
    m_skinnedCelEffect->SetProjection(projection);

    m_goochEffect->SetProjection(projection);
    m_goochEffectNoSpecular->SetProjection(projection);
    m_goochEffectNoRim->SetProjection(projection);
    m_goochEffectVc->SetProjection(projection);
    m_goochEffectCustom->SetProjection(projection);
    m_goochEffectTx->SetProjection(projection);
    m_goochEffectTxNoSpecular->SetProjection(projection);
    m_goochEffectTxNoRim->SetProjection(projection);
    m_goochEffectTxVc->SetProjection(projection);
    m_skinnedGoochEffect->SetProjection(projection);

    m_matcapEffect->SetProjection(projection);
    m_matcapEffectVc->SetProjection(projection);
    m_matcapEffectTx->SetProjection(projection);
    m_matcapEffectTxVc->SetProjection(projection);
    m_skinnedMatcapEffect->SetProjection(projection);
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_celEffect.reset();
    m_celEffectNoSpecular.reset();
    m_celEffectNoRim.reset();
    m_celEffectVc.reset();
    m_celEffectTx.reset();
    m_celEffectTxNoSpecular.reset();
    m_celEffectTxNoRim.reset();
    m_celEffectTxVc.reset();
    m_skinnedCelEffect.reset();

    m_goochEffect.reset();
    m_goochEffectNoSpecular.reset();
    m_goochEffectNoRim.reset();
    m_goochEffectVc.reset();
    m_goochEffectCustom.reset();
    m_goochEffectTx.reset();
    m_goochEffectTxNoSpecular.reset();
    m_goochEffectTxNoRim.reset();
    m_goochEffectTxVc.reset();
    m_skinnedGoochEffect.reset();

    m_matcapEffect.reset();
    m_matcapEffectVc.reset();
    m_matcapEffectTx.reset();
    m_matcapEffectTxVc.reset();
    m_skinnedMatcapEffect.reset();

    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();

    m_refTexture.Reset();
    m_matCapTexture1.Reset();
    m_matCapTexture2.Reset();

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

// Creates a teapot primitive with test input layout.
void Game::CreateTeapot()
{
    VertexCollection vertices;
    IndexCollection indices;

    XMVECTOR negateXZ = XMVectorMultiply(g_XMNegateX, g_XMNegateZ);

    for (size_t i = 0; i < sizeof(TeapotPatches) / sizeof(TeapotPatches[0]); i++)
    {
        TeapotPatch const& patch = TeapotPatches[i];

        // Because the teapot is symmetrical from left to right, we only store
        // data for one side, then tessellate each patch twice, mirroring in X.
        TessellatePatch(vertices, indices, patch, g_XMOne, false);
        TessellatePatch(vertices, indices, patch, g_XMNegateX, true);

        if (patch.mirrorZ)
        {
            // Some parts of the teapot (the body, lid, and rim, but not the
            // handle or spout) are also symmetrical from front to back, so
            // we tessellate them four times, mirroring in Z as well as X.
            TessellatePatch(vertices, indices, patch, g_XMNegateZ, true);
            TessellatePatch(vertices, indices, patch, negateXZ, false);
        }
    }

    // Create the D3D buffers.
    if (vertices.size() >= USHRT_MAX)
        throw std::runtime_error("Too many vertices for 16-bit index buffer");

    // Vertex data
    auto verts = reinterpret_cast<const uint8_t*>(vertices.data());
    size_t vertSizeBytes = vertices.size() * sizeof(TestVertex);

    m_vertexBuffer = GraphicsMemory::Get().Allocate(vertSizeBytes);
    memcpy(m_vertexBuffer.Memory(), verts, vertSizeBytes);

    // Index data
    auto ind = reinterpret_cast<const uint8_t*>(indices.data());
    size_t indSizeBytes = indices.size() * sizeof(uint16_t);

    m_indexBuffer = GraphicsMemory::Get().Allocate(indSizeBytes);
    memcpy(m_indexBuffer.Memory(), ind, indSizeBytes);

    // Record index count for draw
    m_indexCount = static_cast<UINT>(indices.size());

    // Create views
    m_vertexBufferView.BufferLocation = m_vertexBuffer.GpuAddress();
    m_vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(VertexCollection::value_type));
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexBuffer.Size());

    m_indexBufferView.BufferLocation = m_indexBuffer.GpuAddress();
    m_indexBufferView.SizeInBytes = static_cast<UINT>(m_indexBuffer.Size());
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
}
