//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK - HLSL shader coverage
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

#pragma warning(disable : 4238)

//#define GAMMA_CORRECT_RENDERING

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const float ortho_width = 5.f;
    const float ortho_height = 5.f;

    struct TestVertex
    {
        TestVertex(FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate, uint32_t color)
        {
            XMStoreFloat3(&this->position, position);
            XMStoreFloat3(&this->normal, normal);
            XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
            XMStoreFloat2(&this->textureCoordinate2, textureCoordinate * 3);
            XMStoreUByte4(&this->blendIndices, XMVectorSet(0, 0, 0, 0));

            XMStoreFloat3(&this->tangent, g_XMZero);

            XMStoreFloat4(&this->blendWeight, XMVectorSet(1.f, 0.f, 0.f, 0.f));

            this->color = color;
        }

        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT3 tangent;
        XMFLOAT2 textureCoordinate;
        XMFLOAT2 textureCoordinate2;
        XMUBYTE4 blendIndices;
        XMFLOAT4 blendWeight;
        XMUBYTE4 color;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;

    private:
        static const int InputElementCount = 8;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };

    const D3D12_INPUT_ELEMENT_DESC TestVertex::InputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    const D3D12_INPUT_LAYOUT_DESC TestVertex::InputLayout =
    {
        TestVertex::InputElements,
        TestVertex::InputElementCount
    };

    typedef std::vector<TestVertex> VertexCollection;
    typedef std::vector<uint16_t> IndexCollection;


    struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };


    // Helper for computing tangents (see DirectXMesh <http://go.microsoft.com/fwlink/?LinkID=324981>)
    void ComputeTangents(const IndexCollection& indices, VertexCollection& vertices)
    {
        static const float EPSILON = 0.0001f;
        static const XMVECTORF32 s_flips = { 1.f, -1.f, -1.f, 1.f };

        size_t nFaces = indices.size() / 3;
        size_t nVerts = vertices.size();

        std::unique_ptr<XMVECTOR[], aligned_deleter> temp(reinterpret_cast<XMVECTOR*>(_aligned_malloc(sizeof(XMVECTOR) * nVerts * 2, 16)));

        memset(temp.get(), 0, sizeof(XMVECTOR) * nVerts * 2);

        XMVECTOR* tangent1 = temp.get();
        XMVECTOR* tangent2 = temp.get() + nVerts;

        for (size_t face = 0; face < nFaces; ++face)
        {
            uint16_t i0 = indices[face * 3];
            uint16_t i1 = indices[face * 3 + 1];
            uint16_t i2 = indices[face * 3 + 2];

            if (i0 >= nVerts
                || i1 >= nVerts
                || i2 >= nVerts)
            {
                throw std::exception("ComputeTangents");
            }

            XMVECTOR t0 = XMLoadFloat2(&vertices[i0].textureCoordinate);
            XMVECTOR t1 = XMLoadFloat2(&vertices[i1].textureCoordinate);
            XMVECTOR t2 = XMLoadFloat2(&vertices[i2].textureCoordinate);

            XMVECTOR s = XMVectorMergeXY(t1 - t0, t2 - t0);

            XMFLOAT4A tmp;
            XMStoreFloat4A(&tmp, s);

            float d = tmp.x * tmp.w - tmp.z * tmp.y;
            d = (fabsf(d) <= EPSILON) ? 1.f : (1.f / d);
            s *= d;
            s = XMVectorMultiply(s, s_flips);

            XMMATRIX m0;
            m0.r[0] = XMVectorPermute<3, 2, 6, 7>(s, g_XMZero);
            m0.r[1] = XMVectorPermute<1, 0, 4, 5>(s, g_XMZero);
            m0.r[2] = m0.r[3] = g_XMZero;

            XMVECTOR p0 = XMLoadFloat3(&vertices[i0].position);
            XMVECTOR p1 = XMLoadFloat3(&vertices[i1].position);
            XMVECTOR p2 = XMLoadFloat3(&vertices[i2].position);

            XMMATRIX m1;
            m1.r[0] = p1 - p0;
            m1.r[1] = p2 - p0;
            m1.r[2] = m1.r[3] = g_XMZero;

            XMMATRIX uv = XMMatrixMultiply(m0, m1);

            tangent1[i0] = XMVectorAdd(tangent1[i0], uv.r[0]);
            tangent1[i1] = XMVectorAdd(tangent1[i1], uv.r[0]);
            tangent1[i2] = XMVectorAdd(tangent1[i2], uv.r[0]);

            tangent2[i0] = XMVectorAdd(tangent2[i0], uv.r[1]);
            tangent2[i1] = XMVectorAdd(tangent2[i1], uv.r[1]);
            tangent2[i2] = XMVectorAdd(tangent2[i2], uv.r[1]);
        }

        for (size_t j = 0; j < nVerts; ++j)
        {
            // Gram-Schmidt orthonormalization
            XMVECTOR b0 = XMLoadFloat3(&vertices[j].normal);
            b0 = XMVector3Normalize(b0);

            XMVECTOR tan1 = tangent1[j];
            XMVECTOR b1 = tan1 - XMVector3Dot(b0, tan1) * b0;
            b1 = XMVector3Normalize(b1);

            XMVECTOR tan2 = tangent2[j];
            XMVECTOR b2 = tan2 - XMVector3Dot(b0, tan2) * b0 - XMVector3Dot(b1, tan2) * b1;
            b2 = XMVector3Normalize(b2);

            // handle degenerate vectors
            float len1 = XMVectorGetX(XMVector3Length(b1));
            float len2 = XMVectorGetY(XMVector3Length(b2));

            if ((len1 <= EPSILON) || (len2 <= EPSILON))
            {
                if (len1 > 0.5f)
                {
                    // Reset bi-tangent from tangent and normal
                    b2 = XMVector3Cross(b0, b1);
                }
                else if (len2 > 0.5f)
                {
                    // Reset tangent from bi-tangent and normal
                    b1 = XMVector3Cross(b2, b0);
                }
                else
                {
                    // Reset both tangent and bi-tangent from normal
                    XMVECTOR axis;

                    float d0 = fabs(XMVectorGetX(XMVector3Dot(g_XMIdentityR0, b0)));
                    float d1 = fabs(XMVectorGetX(XMVector3Dot(g_XMIdentityR1, b0)));
                    float d2 = fabs(XMVectorGetX(XMVector3Dot(g_XMIdentityR2, b0)));
                    if (d0 < d1)
                    {
                        axis = (d0 < d2) ? g_XMIdentityR0 : g_XMIdentityR2;
                    }
                    else if (d1 < d2)
                    {
                        axis = g_XMIdentityR1;
                    }
                    else
                    {
                        axis = g_XMIdentityR2;
                    }

                    b1 = XMVector3Cross(b0, axis);
                    b2 = XMVector3Cross(b0, b1);
                }
            }

            XMStoreFloat3(&vertices[j].tangent, b1);
        }
    }
}  // anonymous namespace

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
    m_gamePad = std::make_unique<GamePad>();
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

    // Time-based animation
    float time = static_cast<float>(m_timer.GetTotalSeconds());

    float yaw = time * 0.4f;
    float pitch = time * 0.7f;
    float roll = time * 1.1f;

    XMMATRIX world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Setup for cube drawing.
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // BasicEffect
    float y = ortho_height - 0.5f;
    {
        auto it = m_basic.begin();
        assert(it != m_basic.end());

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == m_basic.cend())
                    break;
            }

            if (it == m_basic.cend())
                break;
        }

        // Make sure we drew all the effects
        assert(it == m_basic.cend());

        y -= 1.f;
    }

    // SkinnedEffect
    {
        auto it = m_skinning.begin();
        assert(it != m_skinning.end());

        XMMATRIX bones[4] =
        {
            XMMatrixIdentity(),
            XMMatrixIdentity(),
            XMMatrixIdentity(),
            XMMatrixIdentity(),
        };

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetBoneTransforms(bones, 4);
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == m_skinning.cend())
                    break;
            }

            if (it == m_skinning.cend())
                break;
        }

        // Make sure we drew all the effects
        assert(it == m_skinning.cend());

        y -= 1.f;
    }

    // EnvironmentMapEffect
    {
        auto it = m_envmap.begin();
        assert(it != m_envmap.end());

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == m_envmap.cend())
                    break;
            }

            if (it == m_envmap.cend())
                break;
        }

        // Make sure we drew all the effects
        assert(it == m_envmap.cend());

        y -= 1.f;
    }

    // DualTextureEffect
    {
        auto it = m_dual.begin();
        assert(it != m_dual.end());

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == m_dual.cend())
                    break;
            }

            if (it == m_dual.cend())
                break;
        }

        // Make sure we drew all the effects
        assert(it == m_dual.cend());

        y -= 1.f;
    }

    // AlphaTestEffect
    {
        auto it = m_alphTest.begin();
        assert(it != m_alphTest.end());

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == m_alphTest.cend())
                    break;
            }

            if (it == m_alphTest.cend())
                break;
        }

        // Make sure we drew all the effects
        assert(it == m_alphTest.cend());

        y -= 1.f;
    }

    // NormalMapEffect
    {
        auto it = m_normalMap.begin();
        assert(it != m_normalMap.end());

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == m_normalMap.cend())
                    break;
            }

            if (it == m_normalMap.cend())
                break;
        }

        // Make sure we drew all the effects
        assert(it == m_normalMap.cend());

        y -= 1.f;
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

    CreateCube();

    // Load textures.
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

#ifdef GAMMA_CORRECT_RENDERING
    bool forceSRGB = true;
#else
    bool forceSRGB = false;
#endif

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"cat.dds",
            0, D3D12_RESOURCE_FLAG_NONE, forceSRGB, false,
            m_cat.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_cat.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cat));

    bool iscubemap;
    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"cubemap.dds",
            0, D3D12_RESOURCE_FLAG_NONE, forceSRGB, false,
            m_cubemap.ReleaseAndGetAddressOf(), nullptr, &iscubemap));

    CreateShaderResourceView(device, m_cubemap.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cubemap), iscubemap);

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"overlay.dds",
            0, D3D12_RESOURCE_FLAG_NONE, forceSRGB, false,
            m_overlay.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_overlay.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Overlay));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"default.dds",
            0, D3D12_RESOURCE_FLAG_NONE, forceSRGB, false,
            m_defaultTex.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_defaultTex.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DefaultTex));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"spnza_bricks_a.DDS",
            0, D3D12_RESOURCE_FLAG_NONE, forceSRGB, false,
            m_brickDiffuse.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickDiffuse.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickDiffuse));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"spnza_bricks_a_normal.DDS", m_brickNormal.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickNormal.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickNormal));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"spnza_bricks_a_specular.DDS", m_brickSpecular.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickSpecular.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickSpecular));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    m_deviceResources->WaitForGpu();

    uploadResourcesFinished.wait();

    // Create test effects
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    rtState;

    EffectPipelineStateDescription pd(
        &TestVertex::InputLayout,
        CommonStates::AlphaBlend,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

    auto sampler = m_states->LinearWrap();

    auto defaultTex = m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex);

    //--- BasicEffect ----------------------------------------------------------------------

    // BasicEffect (no texture)
    {
        auto effect =  std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Fog, pd);
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Fog | EffectFlags::VertexColor, pd);
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));
    }

    // BasicEffect (textured)
    {

        auto effect = std::make_unique<BasicEffect>(device, EffectFlags::Texture, pd);
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::Fog, pd);
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::VertexColor, pd);
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::Fog | EffectFlags::VertexColor, pd);
        effect->SetFogColor(Colors::Black);
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));
    }

    // BasicEffect (vertex lighting)
    {
        auto effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::None, pd);
        effect->EnableDefaultLighting();
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Fog | EffectFlags::VertexColor, pd);
        effect->SetFogColor(Colors::Black);
        effect->EnableDefaultLighting();
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::Fog | EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        effect->SetFogColor(Colors::Black);
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));
    }

    // BasicEffect (per pixel light)
    {
        auto effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::None, pd);
        effect->EnableDefaultLighting();
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog | EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Texture, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Texture | EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Texture | EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));

        effect = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Texture | EffectFlags::Fog | EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        effect->SetFogColor(Colors::Black);
        effect->SetTexture(defaultTex, sampler);
        m_basic.emplace_back(std::move(effect));
    }

    //--- SkinnedEFfect --------------------------------------------------------------------

    // SkinnedEFfect (vertex lighting)
    {
        auto effect = std::make_unique<SkinnedEffect>(device, EffectFlags::None, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::None, pd, 2);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::Fog, pd, 2);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::None, pd, 1);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::Fog, pd, 1);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_skinning.emplace_back(std::move(effect));
    }

    // SkinnedEFfect (per pixel lighting)
    {
        auto effect = std::make_unique<SkinnedEffect>(device, EffectFlags::PerPixelLighting, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::PerPixelLighting, pd, 2);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, 2);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::PerPixelLighting, pd, 1);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        m_skinning.emplace_back(std::move(effect));

        effect = std::make_unique<SkinnedEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, 1);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetFogColor(Colors::Black);
        m_skinning.emplace_back(std::move(effect));
    }

    //--- EnvironmentMapEffect -------------------------------------------------------------

    {
        auto envmap = m_resourceDescriptors->GetGpuHandle(Descriptors::Cubemap);

        // EnvironmentMapEffect (fresnel)
        auto effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));

        // EnvironmentMapEffect (no fresnel)
        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Fog, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));

        // EnvironmentMapEffect (specular)
        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pd, false, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Fog, pd, false, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));

        // EnvironmentMapEffect (fresnel + specular)
        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pd, true, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Fog, pd, true, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));

        // EnvironmentMapEffect (per pixel lighting)
        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting, pd, false, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, false, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting, pd, true, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        m_envmap.emplace_back(std::move(effect));

        effect = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, true, true);
        effect->EnableDefaultLighting();
        effect->SetEnvironmentMapSpecular(Colors::Blue);
        effect->SetTexture(defaultTex, sampler);
        effect->SetEnvironmentMap(envmap, sampler);
        effect->SetFogColor(Colors::Black);
        m_envmap.emplace_back(std::move(effect));
    }

    //--- DualTextureEFfect ----------------------------------------------------------------

    {
        auto overlay = m_resourceDescriptors->GetGpuHandle(Descriptors::Overlay);

        auto effect = std::make_unique<DualTextureEffect>(device, EffectFlags::None, pd);
        effect->SetTexture(defaultTex, sampler);
        effect->SetTexture2(overlay, sampler);
        m_dual.emplace_back(std::move(effect));

        effect = std::make_unique<DualTextureEffect>(device, EffectFlags::Fog, pd);
        effect->SetTexture(defaultTex, sampler);
        effect->SetTexture2(overlay, sampler);
        effect->SetFogColor(Colors::Black);
        m_dual.emplace_back(std::move(effect));

        effect = std::make_unique<DualTextureEffect>(device, EffectFlags::VertexColor, pd);
        effect->SetTexture(defaultTex, sampler);
        effect->SetTexture2(overlay, sampler);
        m_dual.emplace_back(std::move(effect));

        effect = std::make_unique<DualTextureEffect>(device, EffectFlags::VertexColor | EffectFlags::Fog, pd);
        effect->SetTexture(defaultTex, sampler);
        effect->SetTexture2(overlay, sampler);
        effect->SetFogColor(Colors::Black);
        m_dual.emplace_back(std::move(effect));
    }

    //--- AlphaTestEffect ------------------------------------------------------------------

    {
        auto cat = m_resourceDescriptors->GetGpuHandle(Descriptors::Cat);

        // AlphaTestEffect (lt/gt)
        auto effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pd);
        effect->SetTexture(cat, sampler);
        m_alphTest.emplace_back(std::move(effect));

        effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::Fog, pd);
        effect->SetTexture(cat, sampler);
        effect->SetFogColor(Colors::Black);
        m_alphTest.emplace_back(std::move(effect));

        effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::VertexColor, pd);
        effect->SetTexture(cat, sampler);
        m_alphTest.emplace_back(std::move(effect));

        effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::VertexColor | EffectFlags::Fog, pd);
        effect->SetTexture(cat, sampler);
        effect->SetFogColor(Colors::Black);
        m_alphTest.emplace_back(std::move(effect));

        // AlphaTestEffect (eg/ne)
        effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pd, D3D12_COMPARISON_FUNC_NOT_EQUAL);
        effect->SetTexture(cat, sampler);
        m_alphTest.emplace_back(std::move(effect));

        effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::Fog, pd, D3D12_COMPARISON_FUNC_NOT_EQUAL);
        effect->SetTexture(cat, sampler);
        effect->SetFogColor(Colors::Black);
        m_alphTest.emplace_back(std::move(effect));

        effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::VertexColor, pd, D3D12_COMPARISON_FUNC_NOT_EQUAL);
        effect->SetTexture(cat, sampler);
        m_alphTest.emplace_back(std::move(effect));

        effect = std::make_unique<AlphaTestEffect>(device, EffectFlags::VertexColor | EffectFlags::Fog, pd, D3D12_COMPARISON_FUNC_NOT_EQUAL);
        effect->SetTexture(cat, sampler);
        effect->SetFogColor(Colors::Black);
        m_alphTest.emplace_back(std::move(effect));
    }

    //--- NormalMapEffect ------------------------------------------------------------------

    {
        auto diffuse = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickDiffuse);
        auto normal = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickNormal);
        auto specular = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickSpecular);

        // NormalMapEffect (no specular)
        auto effect = std::make_unique<NormalMapEffect>(device, EffectFlags::None, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        m_normalMap.emplace_back(std::move(effect));

        effect = std::make_unique<NormalMapEffect>(device, EffectFlags::Fog, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        effect->SetFogColor(Colors::Black);
        m_normalMap.emplace_back(std::move(effect));

        // NormalMapEffect (specular)
        effect = std::make_unique<NormalMapEffect>(device, EffectFlags::None, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        effect->SetSpecularTexture(specular);
        m_normalMap.emplace_back(std::move(effect));

        effect = std::make_unique<NormalMapEffect>(device, EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        effect->SetSpecularTexture(specular);
        effect->SetFogColor(Colors::Black);
        m_normalMap.emplace_back(std::move(effect));

        // NormalMapEffect (vertex color)
        effect = std::make_unique<NormalMapEffect>(device, EffectFlags::VertexColor, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        m_normalMap.emplace_back(std::move(effect));

        effect = std::make_unique<NormalMapEffect>(device, EffectFlags::VertexColor | EffectFlags::Fog, pd, false);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        effect->SetFogColor(Colors::Black);
        m_normalMap.emplace_back(std::move(effect));

        effect = std::make_unique<NormalMapEffect>(device, EffectFlags::VertexColor, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        effect->SetSpecularTexture(specular);
        m_normalMap.emplace_back(std::move(effect));

        effect = std::make_unique<NormalMapEffect>(device, EffectFlags::VertexColor | EffectFlags::Fog, pd);
        effect->EnableDefaultLighting();
        effect->SetTexture(diffuse, sampler);
        effect->SetNormalTexture(normal);
        effect->SetSpecularTexture(specular);
        effect->SetFogColor(Colors::Black);
        m_normalMap.emplace_back(std::move(effect));
    }

    // TODO -
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    XMMATRIX projection = XMMatrixOrthographicRH(ortho_width * 2.f, ortho_height * 2.f, 0.1f, 10.f);

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    XMMATRIX orient = XMLoadFloat4x4(&m_deviceResources->GetOrientationTransform3D());
    projection *= orient;
#endif

    for (auto& it : m_basic)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_skinning)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_envmap)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_dual)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_alphTest)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_normalMap)
    {
        it->SetProjection(projection);
    }
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_basic.clear();
    m_skinning.clear();
    m_envmap.clear();
    m_dual.clear();
    m_alphTest.clear();
    m_normalMap.clear();

    m_cat.Reset();
    m_cubemap.Reset();
    m_overlay.Reset();
    m_defaultTex.Reset();
    m_brickDiffuse.Reset();
    m_brickNormal.Reset();
    m_brickSpecular.Reset();

    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();

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

// Creates a cube primitive with test input layout.
void Game::CreateCube()
{
    VertexCollection vertices;
    IndexCollection indices;

    // A box has six faces, each one pointing in a different direction.
    const int FaceCount = 6;

    static const XMVECTORF32 faceNormals[FaceCount] =
    {
        { 0,  0,  1 },
        { 0,  0, -1 },
        { 1,  0,  0 },
        { -1,  0,  0 },
        { 0,  1,  0 },
        { 0, -1,  0 },
    };

    static const XMVECTORF32 textureCoordinates[4] =
    {
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
        { 0, 0 },
    };

    static uint32_t colors[FaceCount]
    {
        0xFF0000FF,
        0xFF00FF00,
        0xFFFF0000,
        0xFFFF00FF,
        0xFFFFFF00,
        0xFF00FFFF,
    };

    static const XMVECTORF32 tsize = { 0.25f, 0.25f, 0.25f, 0.f };

    // Create each face in turn.
    for (int i = 0; i < FaceCount; i++)
    {
        XMVECTOR normal = faceNormals[i];

        // Get two vectors perpendicular both to the face normal and to each other.
        XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

        XMVECTOR side1 = XMVector3Cross(normal, basis);
        XMVECTOR side2 = XMVector3Cross(normal, side1);

        // Six indices (two triangles) per face.
        size_t vbase = vertices.size();
        indices.push_back(uint16_t(vbase + 0));
        indices.push_back(uint16_t(vbase + 1));
        indices.push_back(uint16_t(vbase + 2));

        indices.push_back(uint16_t(vbase + 0));
        indices.push_back(uint16_t(vbase + 2));
        indices.push_back(uint16_t(vbase + 3));

        // Four vertices per face.
        vertices.push_back(TestVertex((normal - side1 - side2) * tsize, normal, textureCoordinates[0], colors[i]));
        vertices.push_back(TestVertex((normal - side1 + side2) * tsize, normal, textureCoordinates[1], colors[i]));
        vertices.push_back(TestVertex((normal + side1 + side2) * tsize, normal, textureCoordinates[2], colors[i]));
        vertices.push_back(TestVertex((normal + side1 - side2) * tsize, normal, textureCoordinates[3], colors[i]));
    }

    // Compute tangents
    ComputeTangents(indices, vertices);

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
