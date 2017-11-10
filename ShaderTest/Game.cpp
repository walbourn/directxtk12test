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

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const float SWAP_TIME = 10.f;

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

            XMVECTOR clr = XMLoadUByteN4(reinterpret_cast<XMUBYTEN4*>(&color));
            XMStoreFloat4(&this->color, clr);
        }

        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT3 tangent;
        XMFLOAT2 textureCoordinate;
        XMFLOAT2 textureCoordinate2;
        XMUBYTE4 blendIndices;
        XMFLOAT4 blendWeight;
        XMFLOAT4 color;

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
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
    
    struct TestCompressedVertex
    {
        TestCompressedVertex(const TestVertex& bn)
        {
            position = bn.position;
            blendIndices = bn.blendIndices;

            XMVECTOR v = XMLoadFloat3(&bn.normal);
            v = v * g_XMOneHalf;
            v += g_XMOneHalf;
            XMStoreFloat3PK(&this->normal, v);

            v = XMLoadFloat3(&bn.tangent);
            v = v * g_XMOneHalf;
            v += g_XMOneHalf;
            XMStoreFloat3PK(&this->tangent, v);

            v = XMLoadFloat2(&bn.textureCoordinate);
            XMStoreHalf2(&this->textureCoordinate, v);

            v = XMLoadFloat2(&bn.textureCoordinate2);
            XMStoreHalf2(&this->textureCoordinate2, v);

            v = XMLoadFloat4(&bn.blendWeight);
            XMStoreUByteN4(&this->blendWeight, v);

            v = XMLoadFloat4(&bn.color);
            XMStoreColor(&this->color, v);
        }

        XMFLOAT3 position;
        XMFLOAT3PK normal;
        XMFLOAT3PK tangent;
        XMHALF2 textureCoordinate;
        XMHALF2 textureCoordinate2;
        XMUBYTE4 blendIndices;
        XMUBYTEN4 blendWeight;
        XMCOLOR color;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;

    private:
        static const int InputElementCount = 8;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };

    const D3D12_INPUT_ELEMENT_DESC TestCompressedVertex::InputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R11G11B10_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R11G11B10_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R16G16_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     1, DXGI_FORMAT_R16G16_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,   0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    const D3D12_INPUT_LAYOUT_DESC TestCompressedVertex::InputLayout =
    {
        TestCompressedVertex::InputElements,
        TestCompressedVertex::InputElementCount
    };
}  // anonymous namespace

Game::Game() :
    m_showCompressed(false),
    m_delay(0)
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

    m_delay = SWAP_TIME;
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

    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    m_keyboardButtons.Update(kb);

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space) || (m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED))
    {
        m_showCompressed = !m_showCompressed;
        m_delay = SWAP_TIME;
    }
    else if (!kb.Space && !(pad.IsConnected() && pad.IsYPressed()))
    {
        m_delay -= static_cast<float>(timer.GetElapsedSeconds());

        if (m_delay <= 0.f)
        {
            m_showCompressed = !m_showCompressed;
            m_delay = SWAP_TIME;
        }
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
    commandList->IASetVertexBuffers(0, 1, (m_showCompressed) ? &m_vertexBufferViewBn : &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // BasicEffect
    float y = ortho_height - 0.5f;
    {
        auto it = (m_showCompressed) ? m_basicBn.cbegin() : m_basic.cbegin();
        auto eit = (m_showCompressed) ? m_basicBn.cend() : m_basic.cend();
        assert(it != eit);

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == eit)
                    break;
            }

            if (it == eit)
                break;
        }

        // Make sure we drew all the effects
        assert(it == eit);

        y -= 1.f;
    }

    // SkinnedEffect
    {
        auto it = (m_showCompressed) ? m_skinningBn.cbegin() : m_skinning.cbegin();
        auto eit = (m_showCompressed) ? m_skinningBn.cend() : m_skinning.cend();
        assert(it != eit);

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
                if (it == eit)
                    break;
            }

            if (it == eit)
                break;
        }

        // Make sure we drew all the effects
        assert(it == eit);

        y -= 1.f;
    }

    // EnvironmentMapEffect
    {
        auto it = (m_showCompressed) ? m_envmapBn.cbegin() : m_envmap.cbegin();
        auto eit = (m_showCompressed) ? m_envmapBn.cend() : m_envmap.cend();
        assert(it != eit);

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == eit)
                    break;
            }

            if (it == eit)
                break;
        }

        // Make sure we drew all the effects
        assert(it == eit);

        y -= 1.f;
    }

    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

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

    commandList->IASetVertexBuffers(0, 1, (m_showCompressed) ? &m_vertexBufferViewBn : &m_vertexBufferView);

    // NormalMapEffect
    {
        auto it = (m_showCompressed) ? m_normalMapBn.cbegin() : m_normalMap.cbegin();
        auto eit = (m_showCompressed) ? m_normalMapBn.cend() : m_normalMap.cend();
        assert(it != eit);

        for (; y > -ortho_height; y -= 1.f)
        {
            for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
            {
                (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                ++it;
                if (it == eit)
                    break;
            }

            if (it == eit)
                break;
        }

        // Make sure we drew all the effects
        assert(it == eit);

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
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
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
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
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
    unsigned int loadFlags = DDS_LOADER_FORCE_SRGB;
#else
    unsigned int loadFlags = DDS_LOADER_DEFAULT;
#endif

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"cat.dds",
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_cat.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_cat.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cat));

    bool iscubemap;
    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"cubemap.dds",
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_cubemap.ReleaseAndGetAddressOf(), nullptr, &iscubemap));

    CreateShaderResourceView(device, m_cubemap.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Cubemap), iscubemap);

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"overlay.dds",
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_overlay.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_overlay.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Overlay));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"default.dds",
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_defaultTex.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_defaultTex.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DefaultTex));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"spnza_bricks_a.DDS",
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_brickDiffuse.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickDiffuse.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickDiffuse));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"spnza_bricks_a_normal.DDS", m_brickNormal.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickNormal.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickNormal));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"spnza_bricks_a_specular.DDS", m_brickSpecular.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickSpecular.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickSpecular));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();

    // Create test effects
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    rtState;

    auto sampler = m_states->LinearWrap();

    auto defaultTex = m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex);

    for (int j = 0; j < 2; ++j)
    {
        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::AlphaBlend,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        pd.inputLayout = (!j) ? TestVertex::InputLayout : TestCompressedVertex::InputLayout;

        unsigned int eflags = (!j) ? EffectFlags::None : EffectFlags::BiasedVertexNormals;

        //--- BasicEffect ------------------------------------------------------------------

        {
            std::vector<std::unique_ptr<DirectX::BasicEffect>> basic;

            // BasicEffect (no texture)
            {
                auto effect = std::make_unique<BasicEffect>(device, eflags, pd);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Fog, pd);
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::VertexColor, pd);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Fog | EffectFlags::VertexColor, pd);
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));
            }

            // BasicEffect (textured)
            {
                auto effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Texture, pd);
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Texture | EffectFlags::Fog, pd);
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Texture | EffectFlags::VertexColor, pd);
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Texture | EffectFlags::Fog | EffectFlags::VertexColor, pd);
                effect->SetFogColor(Colors::Black);
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));
            }

            // BasicEffect (vertex lighting)
            {
                auto effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting, pd);
                effect->EnableDefaultLighting();
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting | EffectFlags::Fog, pd);
                effect->EnableDefaultLighting();
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting | EffectFlags::VertexColor, pd);
                effect->EnableDefaultLighting();
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting | EffectFlags::Fog | EffectFlags::VertexColor, pd);
                effect->SetFogColor(Colors::Black);
                effect->EnableDefaultLighting();
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting | EffectFlags::Texture, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::Fog, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::VertexColor, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::Fog | EffectFlags::VertexColor, pd);
                effect->EnableDefaultLighting();
                effect->SetFogColor(Colors::Black);
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));
            }

            // BasicEffect (per pixel light)
            {
                auto effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting, pd);
                effect->EnableDefaultLighting();
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd);
                effect->EnableDefaultLighting();
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::VertexColor, pd);
                effect->EnableDefaultLighting();
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog | EffectFlags::VertexColor, pd);
                effect->EnableDefaultLighting();
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Texture, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Texture | EffectFlags::Fog, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Texture | EffectFlags::VertexColor, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));

                effect = std::make_unique<BasicEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Texture | EffectFlags::Fog | EffectFlags::VertexColor, pd);
                effect->EnableDefaultLighting();
                effect->SetFogColor(Colors::Black);
                effect->SetTexture(defaultTex, sampler);
                basic.emplace_back(std::move(effect));
            }

            if (!j)
            {
                m_basic.swap(basic);
            }
            else
            {
                m_basicBn.swap(basic);
            }
        }

        //--- SkinnedEFfect ----------------------------------------------------------------

        {
            std::vector<std::unique_ptr<DirectX::SkinnedEffect>> skinning;

            // SkinnedEFfect (vertex lighting)
            {
                auto effect = std::make_unique<SkinnedEffect>(device, eflags, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::Fog, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags, pd, 2);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::Fog, pd, 2);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags, pd, 1);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::Fog, pd, 1);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                skinning.emplace_back(std::move(effect));
            }

            // SkinnedEFfect (per pixel lighting)
            {
                auto effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::PerPixelLighting, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, 2);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, 2);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, 1);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                skinning.emplace_back(std::move(effect));

                effect = std::make_unique<SkinnedEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, 1);
                effect->EnableDefaultLighting();
                effect->SetTexture(defaultTex, sampler);
                effect->SetFogColor(Colors::Black);
                skinning.emplace_back(std::move(effect));
            }

            if (!j)
            {
                m_skinning.swap(skinning);
            }
            else
            {
                m_skinningBn.swap(skinning);
            }
        }

        //--- EnvironmentMapEffect ---------------------------------------------------------

        {
            auto envmap = m_resourceDescriptors->GetGpuHandle(Descriptors::Cubemap);

            std::vector<std::unique_ptr<DirectX::EnvironmentMapEffect>> envmaps;

            // EnvironmentMapEffect (fresnel)
            auto effect = std::make_unique<EnvironmentMapEffect>(device, eflags, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect (no fresnel)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fog, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect (specular)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags, pd, false, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fog, pd, false, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect (fresnel + specular)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags, pd, true, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fog, pd, true, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect (per pixel lighting)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, false, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, false, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, true, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, true, true);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            if (!j)
            {
                m_envmap.swap(envmaps);
            }
            else
            {
                m_envmapBn.swap(envmaps);
            }
        }

        //--- NormalMapEffect --------------------------------------------------------------

        {
            auto diffuse = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickDiffuse);
            auto normal = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickNormal);
            auto specular = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickSpecular);

            std::vector<std::unique_ptr<DirectX::NormalMapEffect>> normalMap;

            // NormalMapEffect (no specular)
            auto effect = std::make_unique<NormalMapEffect>(device, eflags, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Fog, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            // NormalMapEffect (specular)
            effect = std::make_unique<NormalMapEffect>(device, eflags, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            // NormalMapEffect (vertex color)
            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor | EffectFlags::Fog, pd, false);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            if (!j)
            {
                m_normalMap.swap(normalMap);
            }
            else
            {
                m_normalMapBn.swap(normalMap);
            }
        }
    }

    EffectPipelineStateDescription pd(
        &TestVertex::InputLayout,
        CommonStates::AlphaBlend,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

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

    for (auto& it : m_basicBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_skinning)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_skinningBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_envmap)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_envmapBn)
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

    for (auto& it : m_normalMapBn)
    {
        it->SetProjection(projection);
    }
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_basic.clear();
    m_basicBn.clear();
    m_skinning.clear();
    m_skinningBn.clear();
    m_envmap.clear();
    m_envmapBn.clear();
    m_dual.clear();
    m_alphTest.clear();
    m_normalMap.clear();
    m_normalMapBn.clear();

    m_cat.Reset();
    m_cubemap.Reset();
    m_overlay.Reset();
    m_defaultTex.Reset();
    m_brickDiffuse.Reset();
    m_brickNormal.Reset();
    m_brickSpecular.Reset();

    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_vertexBufferBn.Reset();

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
    {
        auto verts = reinterpret_cast<const uint8_t*>(vertices.data());
        size_t vertSizeBytes = vertices.size() * sizeof(TestVertex);

        m_vertexBuffer = GraphicsMemory::Get().Allocate(vertSizeBytes);
        memcpy(m_vertexBuffer.Memory(), verts, vertSizeBytes);
    }

    // Compressed Vertex data
    {
        std::vector<TestCompressedVertex> cvertices;
        cvertices.reserve(vertices.size());
        for (auto&i : vertices)
        {
            TestCompressedVertex cv(i);
            cvertices.emplace_back(std::move(cv));
        }

        auto verts = reinterpret_cast<const uint8_t*>(cvertices.data());
        size_t vertSizeBytes = cvertices.size() * sizeof(TestCompressedVertex);

        m_vertexBufferBn = GraphicsMemory::Get().Allocate(vertSizeBytes);
        memcpy(m_vertexBufferBn.Memory(), verts, vertSizeBytes);
    }

    // Index data
    auto ind = reinterpret_cast<const uint8_t*>(indices.data());
    size_t indSizeBytes = indices.size() * sizeof(uint16_t);

    m_indexBuffer = GraphicsMemory::Get().Allocate(indSizeBytes);
    memcpy(m_indexBuffer.Memory(), ind, indSizeBytes);

    // Record index count for draw
    m_indexCount = static_cast<UINT>(indices.size());

    // Create views
    m_vertexBufferView.BufferLocation = m_vertexBuffer.GpuAddress();
    m_vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(TestVertex));
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexBuffer.Size());

    m_vertexBufferViewBn.BufferLocation = m_vertexBufferBn.GpuAddress();
    m_vertexBufferViewBn.StrideInBytes = static_cast<UINT>(sizeof(TestCompressedVertex));
    m_vertexBufferViewBn.SizeInBytes = static_cast<UINT>(m_vertexBufferBn.Size());

    m_indexBufferView.BufferLocation = m_indexBuffer.GpuAddress();
    m_indexBufferView.SizeInBytes = static_cast<UINT>(m_indexBuffer.Size());
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
}
