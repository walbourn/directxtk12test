//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include "Bezier.h"

// Build for LH vs. RH coords
//#define LH_COORDS

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    struct TestVertex
    {
        TestVertex(FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate)
        {
            XMStoreFloat3(&this->position, position);
            XMStoreFloat3(&this->normal, normal);
            XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
            XMStoreFloat2(&this->textureCoordinate2, textureCoordinate * 3);
            XMStoreUByte4(&this->blendIndices, XMVectorSet(0, 1, 2, 3));

            float u = XMVectorGetX(textureCoordinate) - 0.5f;
            float v = XMVectorGetY(textureCoordinate) - 0.5f;

            float d = 1 - sqrt(u * u + v * v) * 2;

            if (d < 0)
                d = 0;

            XMStoreFloat4(&this->blendWeight, XMVectorSet(d, 1 - d, u, v));
        }

        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT2 textureCoordinate;
        XMFLOAT2 textureCoordinate2;
        XMUBYTE4 blendIndices;
        XMFLOAT4 blendWeight;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;

    private:
        static const int InputElementCount = 6;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };

    const D3D12_INPUT_ELEMENT_DESC TestVertex::InputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
        const int tessellation = 16;

        // Look up the 16 control points for this patch.
        XMVECTOR controlPoints[16];

        for (int i = 0; i < 16; i++)
        {
            controlPoints[i] = TeapotControlPoints[patch.indices[i]] * scale;
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
} // anonymous namespace

Game::Game()
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    m_deviceResources->WaitForGpu();
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_keyboard = std::make_unique<Keyboard>();

    m_deviceResources->SetWindow(window, width, height);

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
void Game::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto kb = m_keyboard->GetState();

    if (kb.Escape)
    {
        PostQuitMessage(0);
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
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(1, heaps);

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

    //--- BasicEFfect ----------------------------------------------------------------------
    // Simple unlit teapot.
    m_basicEffectUnlit->SetAlpha(1.f);
    m_basicEffectUnlit->SetWorld(world * XMMatrixTranslation(-4, 2.5f, 0));
    m_basicEffectUnlit->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Unlit with alpha fading.
    m_basicEffectUnlit->SetAlpha(alphaFade);
    m_basicEffectUnlit->SetWorld(world * XMMatrixTranslation(-4, 1.5f, 0));
    m_basicEffectUnlit->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Unlit with fog.
    m_basicEffectUnlitFog->SetWorld(world * XMMatrixTranslation(-4, 0.5f, 2 - alphaFade * 6));
    m_basicEffectUnlitFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit teapot.
    m_basicEffect->SetAlpha(1.f);
    m_basicEffect->SetWorld(world * XMMatrixTranslation(-3, 2.5f, 0));
    m_basicEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit teapot, no specular.
    m_basicEffectNoSpecular->SetAlpha(1.f);
    m_basicEffectNoSpecular->SetWorld(world * XMMatrixTranslation(-2, 0, 0));
    m_basicEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit with alpha fading.
    m_basicEffect->SetAlpha(alphaFade);
    m_basicEffect->SetWorld(world * XMMatrixTranslation(-3, 1.5f, 0));
    m_basicEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit alpha fading, no specular.
    m_basicEffectNoSpecular->SetAlpha(alphaFade);
    m_basicEffectNoSpecular->SetWorld(world * XMMatrixTranslation(-1, 0, 0));
    m_basicEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit with fog.
    m_basicEffectFog->SetAlpha(1.f);
    m_basicEffectFog->SetWorld(world * XMMatrixTranslation(-3, 0.5f, 2 - alphaFade * 6));
    m_basicEffectFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    {
        // Light only from above.
        m_basicEffect->SetAlpha(1.f);
        m_basicEffect->SetLightDirection(0, XMVectorSet(0, -1, 0, 0));
        m_basicEffect->SetLightEnabled(1, false);
        m_basicEffect->SetLightEnabled(2, false);
        m_basicEffect->SetWorld(world *  XMMatrixTranslation(-2, 2.5f, 0));
        m_basicEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from the left.
        m_basicEffect->SetLightDirection(0, XMVectorSet(1, 0, 0, 0));
        m_basicEffect->SetWorld(world * XMMatrixTranslation(-1, 2.5f, 0));
        m_basicEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from straight in front.
        m_basicEffect->SetLightDirection(0, XMVectorSet(0, 0, -1, 0));
        m_basicEffect->SetWorld(world * XMMatrixTranslation(0, 2.5f, 0));
        m_basicEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }

    m_basicEffect->EnableDefaultLighting();

    // Non uniform scaling.
    m_basicEffect->SetWorld(XMMatrixScaling(1, 2, 0.25f) * world * XMMatrixTranslation(1, 2.5f, 0));
    m_basicEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    {
        // Light only from above + per pixel lighting.
        m_basicEffectPPL->SetLightDirection(0, XMVectorSet(0, -1, 0, 0));
        m_basicEffectPPL->SetLightEnabled(1, false);
        m_basicEffectPPL->SetLightEnabled(2, false);
        m_basicEffectPPL->SetWorld(world *  XMMatrixTranslation(-2, 1.5f, 0));
        m_basicEffectPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from the left + per pixel lighting.
        m_basicEffectPPL->SetLightDirection(0, XMVectorSet(1, 0, 0, 0));
        m_basicEffectPPL->SetWorld(world * XMMatrixTranslation(-1, 1.5f, 0));
        m_basicEffectPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from straight in front + per pixel lighting.
        m_basicEffectPPL->SetLightDirection(0, XMVectorSet(0, 0, -1, 0));
        m_basicEffectPPL->SetWorld(world * XMMatrixTranslation(0, 1.5f, 0));
        m_basicEffectPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        m_basicEffectPPL->EnableDefaultLighting();

        // Non uniform scaling + per pixel lighting.
        m_basicEffectPPL->SetWorld(XMMatrixScaling(1, 2, 0.25f) * world * XMMatrixTranslation(1, 1.5f, 0));
        m_basicEffectPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }

    //--- SkinnedEFfect --------------------------------------------------------------------
    XMMATRIX bones[4] =
    {
        XMMatrixIdentity(),
        XMMatrixIdentity(),
        XMMatrixScaling(0, 0, 0),
        XMMatrixScaling(0, 0, 0),
    };

    m_skinnedEffect->SetBoneTransforms(bones, 4);
    m_skinnedEffect->SetWorld(world);
    m_skinnedEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_skinnedEffectNoSpecular->SetBoneTransforms(bones, 4);
    m_skinnedEffectNoSpecular->SetWorld(world * XMMatrixTranslation(1, 0, 0));
    m_skinnedEffectNoSpecular->Apply(commandList);
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

    m_skinnedEffect->SetBoneTransforms(bones, 4);
    m_skinnedEffect->SetWorld(world * XMMatrixTranslation(-1, -2, 0));
    m_skinnedEffect->Apply(commandList);
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

    m_skinnedEffect->SetBoneTransforms(bones, 4);
    m_skinnedEffect->SetWorld(world * XMMatrixTranslation(-3, -2, 0));
    m_skinnedEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- EnvironmentMapEffect -------------------------------------------------------------
    m_envmap->SetAlpha(1.f);
    m_envmap->SetEnvironmentMapAmount(1.f);
    m_envmap->SetFresnelFactor(1.f);
    m_envmap->SetWorld(world * XMMatrixTranslation(2, 2.5f, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map with alpha fading.
    m_envmap->SetAlpha(alphaFade);
    m_envmap->SetWorld(world * XMMatrixTranslation(2, 1.5f, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map with fog.
    m_envmapFog->SetWorld(world * XMMatrixTranslation(2, 0.5f, 2 - alphaFade * 6));
    m_envmapFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map, animating the fresnel factor.
    m_envmap->SetAlpha(1.f);
    m_envmap->SetFresnelFactor(alphaFade * 3);
    m_envmap->SetWorld(world * XMMatrixTranslation(2, -0.5f, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map, animating the amount.
    m_envmap->SetEnvironmentMapAmount(alphaFade);
    m_envmap->SetWorld(world * XMMatrixTranslation(2, -1.5f, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map, animating the amount, with no fresnel.
    m_envmapNoFresnel->SetEnvironmentMapAmount(alphaFade);
    m_envmapNoFresnel->SetWorld(world * XMMatrixTranslation(2, -2.5f, 0));
    m_envmapNoFresnel->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- DualTextureEFfect ----------------------------------------------------------------
    m_dualTexture->SetAlpha(1.f);
    m_dualTexture->SetWorld(world *  XMMatrixTranslation(3, 2.5f, 0));
    m_dualTexture->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Dual texture with alpha fading.
    m_dualTexture->SetAlpha(alphaFade);
    m_dualTexture->SetWorld(world *  XMMatrixTranslation(3, 1.5f, 0));
    m_dualTexture->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Dual texture with fog.
    m_dualTextureFog->SetWorld(world *  XMMatrixTranslation(3, 0.5f, 2 - alphaFade * 6));
    m_dualTextureFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- AlphaTestEFfect ------------------------------------------------------------------

    {
        // Alpha test, > 0.
        m_alphaTest->SetReferenceAlpha(0);
        m_alphaTest->SetWorld(world * XMMatrixTranslation(4, 2.5f, 0));
        m_alphaTest->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, > 128.
        m_alphaTest->SetReferenceAlpha(128);
        m_alphaTest->SetWorld(world * XMMatrixTranslation(4, 1.5f, 0));
        m_alphaTest->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test with fog.
        m_alphaTestFog->SetReferenceAlpha(128);
        m_alphaTestFog->SetWorld(world * XMMatrixTranslation(4, 0.5f, 2 - alphaFade * 6));
        m_alphaTestFog->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, < animating value.
        m_alphaTestLess->SetReferenceAlpha(1 + (int)(alphaFade * 254));
        m_alphaTestLess->SetWorld(world * XMMatrixTranslation(4, -0.5f, 0));
        m_alphaTestLess->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, = 255.
        m_alphaTestEqual->SetReferenceAlpha(255);
        m_alphaTestEqual->SetWorld(world * XMMatrixTranslation(4, -1.5f, 0));
        m_alphaTestEqual->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, != 0.
        m_alphaTestNotEqual->SetReferenceAlpha(0);
        m_alphaTestNotEqual->SetWorld(world * XMMatrixTranslation(4, -2.5f, 0));
        m_alphaTestNotEqual->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }

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
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
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

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
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

    CreateTeapot();

    // Create test effects
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

#ifdef LH_COORDS
    const float fogstart = -6;
    const float fogend = -8;
#else
    const float fogstart = 6;
    const float fogend = 8;
#endif

    {
        EffectPipelineStateDescription pdAlpha(
            &TestVertex::InputLayout,
            CommonStates::AlphaBlend,
            CommonStates::DepthDefault,
#ifdef LH_COORDS
            CommonStates::CullClockwise,
#else
            CommonStates::CullCounterClockwise,
#endif
            rtState);

        EffectPipelineStateDescription pdOpaque(
            &TestVertex::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
#ifdef LH_COORDS
            CommonStates::CullClockwise,
#else
            CommonStates::CullCounterClockwise,
#endif
            rtState);

        //--- BasicEFfect ------------------------------------------------------------------
        m_basicEffectUnlit = std::make_unique<BasicEffect>(device, EffectFlags::None, pdAlpha);
        m_basicEffectUnlit->SetDiffuseColor(Colors::Blue);

        m_basicEffectUnlitFog = std::make_unique<BasicEffect>(device, EffectFlags::Fog, pdAlpha);
        m_basicEffectUnlitFog->SetDiffuseColor(Colors::Blue);
        m_basicEffectUnlitFog->SetFogColor(Colors::Gray);
        m_basicEffectUnlitFog->SetFogStart(fogstart);
        m_basicEffectUnlitFog->SetFogEnd(fogend);

        m_basicEffect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pdAlpha);
        m_basicEffect->EnableDefaultLighting();
        m_basicEffect->SetDiffuseColor(Colors::Red);

        m_basicEffectFog = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Fog, pdAlpha);
        m_basicEffectFog->EnableDefaultLighting();
        m_basicEffectFog->SetDiffuseColor(Colors::Red);
        m_basicEffectFog->SetFogColor(Colors::Gray);
        m_basicEffectFog->SetFogStart(fogstart);
        m_basicEffectFog->SetFogEnd(fogend);

        m_basicEffectNoSpecular = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pdAlpha);
        m_basicEffectNoSpecular->EnableDefaultLighting();
        m_basicEffectNoSpecular->SetDiffuseColor(Colors::Red);
        m_basicEffectNoSpecular->DisableSpecular();

        m_basicEffectPPL = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting, pdAlpha);
        m_basicEffectPPL->EnableDefaultLighting();
        m_basicEffectPPL->SetDiffuseColor(Colors::Red);
        
        //--- SkinnedEFfect ----------------------------------------------------------------
        m_skinnedEffect = std::make_unique<SkinnedEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pdAlpha);
        m_skinnedEffect->EnableDefaultLighting();

        m_skinnedEffectNoSpecular = std::make_unique<SkinnedEffect>(device, EffectFlags::Lighting, pdAlpha);
        m_skinnedEffectNoSpecular->EnableDefaultLighting();
        m_skinnedEffectNoSpecular->DisableSpecular();

        //--- EnvironmentMapEffect ---------------------------------------------------------
        m_envmap = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pdAlpha);
        m_envmap->EnableDefaultLighting();

        m_envmapFog = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Fog, pdAlpha);
        m_envmapFog->EnableDefaultLighting();
        m_envmapFog->SetFogColor(Colors::Gray);
        m_envmapFog->SetFogStart(fogstart);
        m_envmapFog->SetFogEnd(fogend);

        m_envmapNoFresnel = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pdAlpha, false);
        m_envmapNoFresnel->EnableDefaultLighting();

        //--- DualTextureEFfect ------------------------------------------------------------
        m_dualTexture = std::make_unique<DualTextureEffect>(device, EffectFlags::None, pdAlpha);

        m_dualTextureFog = std::make_unique<DualTextureEffect>(device, EffectFlags::Fog, pdAlpha);
        m_dualTextureFog->SetFogColor(Colors::Gray);
        m_dualTextureFog->SetFogStart(fogstart);
        m_dualTextureFog->SetFogEnd(fogend);

        //--- AlphaTestEFfect --------------------------------------------------------------
        m_alphaTest = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque);

        m_alphaTestFog = std::make_unique<AlphaTestEffect>(device, EffectFlags::Fog, pdOpaque);
        m_alphaTestFog->SetFogColor(Colors::Red);
        m_alphaTestFog->SetFogStart(fogstart);
        m_alphaTestFog->SetFogEnd(fogend);

        m_alphaTestLess = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque, D3D12_COMPARISON_FUNC_LESS);

        m_alphaTestEqual = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque, D3D12_COMPARISON_FUNC_EQUAL);

        m_alphaTestNotEqual = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque, D3D12_COMPARISON_FUNC_NOT_EQUAL);
    }

    // Load textures.
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"cat.dds", m_cat.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_cat.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cat));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"opaqueCat.dds", m_opaqueCat.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_opaqueCat.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::OpaqueCat));

    bool iscubemap;
    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"cubemap.dds", m_cubemap.ReleaseAndGetAddressOf(), false, 0, nullptr, &iscubemap));

    CreateShaderResourceView(device, m_cubemap.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cubemap), iscubemap);

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"overlay.dds", m_overlay.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_overlay.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Overlay));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    m_deviceResources->WaitForGpu();

    uploadResourcesFinished.wait();

    // Set textures.
    auto opaqueCat = m_resourceDescriptors->GetGpuHandle(Descriptors::OpaqueCat);

    m_skinnedEffect->SetTexture(opaqueCat);
    m_skinnedEffectNoSpecular->SetTexture(opaqueCat);

    auto cubemap = m_resourceDescriptors->GetGpuHandle(Descriptors::Cubemap);

    m_envmap->SetTexture(opaqueCat);
    m_envmap->SetEnvironmentMap(cubemap);
    m_envmapFog->SetTexture(opaqueCat);
    m_envmapFog->SetEnvironmentMap(cubemap);
    m_envmapNoFresnel->SetTexture(opaqueCat);
    m_envmapNoFresnel->SetEnvironmentMap(cubemap);

    auto overlay = m_resourceDescriptors->GetGpuHandle(Descriptors::Overlay);

    m_dualTexture->SetTexture(opaqueCat);
    m_dualTexture->SetTexture2(overlay);
    m_dualTextureFog->SetTexture(opaqueCat);
    m_dualTextureFog->SetTexture2(overlay);

    auto cat = m_resourceDescriptors->GetGpuHandle(Descriptors::Cat);

    m_alphaTest->SetTexture(cat);
    m_alphaTestFog->SetTexture(cat);
    m_alphaTestLess->SetTexture(cat);
    m_alphaTestEqual->SetTexture(cat);
    m_alphaTestNotEqual->SetTexture(cat);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { 0, 0, 6 };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 15);
#else
    XMMATRIX view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 15);
#endif

    m_basicEffectUnlit->SetView(view);
    m_basicEffectUnlitFog->SetView(view);
    m_basicEffect->SetView(view);
    m_basicEffectFog->SetView(view);
    m_basicEffectNoSpecular->SetView(view);
    m_basicEffectPPL->SetView(view);

    m_skinnedEffect->SetView(view);
    m_skinnedEffectNoSpecular->SetView(view);

    m_envmap->SetView(view);
    m_envmapFog->SetView(view);
    m_envmapNoFresnel->SetView(view);

    m_dualTexture->SetView(view);
    m_dualTextureFog->SetView(view);

    m_alphaTest->SetView(view);
    m_alphaTestFog->SetView(view);
    m_alphaTestLess->SetView(view);
    m_alphaTestEqual->SetView(view);
    m_alphaTestNotEqual->SetView(view);

    m_basicEffectUnlit->SetProjection(projection);
    m_basicEffectUnlitFog->SetProjection(projection);
    m_basicEffect->SetProjection(projection);
    m_basicEffectFog->SetProjection(projection);
    m_basicEffectNoSpecular->SetProjection(projection);
    m_basicEffectPPL->SetProjection(projection);

    m_skinnedEffect->SetProjection(projection);
    m_skinnedEffectNoSpecular->SetProjection(projection);

    m_envmap->SetProjection(projection);
    m_envmapFog->SetProjection(projection);
    m_envmapNoFresnel->SetProjection(projection);

    m_dualTexture->SetProjection(projection);
    m_dualTextureFog->SetProjection(projection);

    m_alphaTest->SetProjection(projection);
    m_alphaTestFog->SetProjection(projection);
    m_alphaTestLess->SetProjection(projection);
    m_alphaTestEqual->SetProjection(projection);
    m_alphaTestNotEqual->SetProjection(projection);
}

void Game::OnDeviceLost()
{
    m_basicEffectUnlit.reset();
    m_basicEffectUnlitFog.reset();
    m_basicEffect.reset();
    m_basicEffectFog.reset();
    m_basicEffectNoSpecular.reset();
    m_basicEffectPPL.reset();

    m_skinnedEffect.reset();
    m_skinnedEffectNoSpecular.reset();

    m_envmap.reset();
    m_envmapFog.reset();
    m_envmapNoFresnel.reset();

    m_dualTexture.reset();
    m_dualTextureFog.reset();

    m_alphaTest.reset();
    m_alphaTestFog.reset();
    m_alphaTestLess.reset();
    m_alphaTestEqual.reset();
    m_alphaTestNotEqual.reset();

    m_cat.Reset();
    m_opaqueCat.Reset();
    m_cubemap.Reset();
    m_overlay.Reset();

    m_resourceDescriptors.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

// Creates a teapot primitive with test input layout.
void Game::CreateTeapot()
{
    VertexCollection vertices;
    IndexCollection indices;

    for (int i = 0; i < sizeof(TeapotPatches) / sizeof(TeapotPatches[0]); i++)
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
            TessellatePatch(vertices, indices, patch, g_XMNegateX * g_XMNegateZ, false);
        }
    }

    // Create the D3D buffers.
    if (vertices.size() >= USHRT_MAX)
        throw std::exception("Too many vertices for 16-bit index buffer");

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
