//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK Effects
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
#include "Bezier.h"

#pragma warning( disable : 4238 )

#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const float rowA = -2.f;
    const float row0 = 2.5f;
    const float row1 = 1.5f;
    const float row2 = 0.5f;
    const float row3 = 0.f;
    const float row4 = -0.5f;
    const float row5 = -1.5f;
    const float row6 = -2.5f;

    const float colA = -5.f;
    const float col0 = -4.f;
    const float col1 = -3.f;
    const float col2 = -2.f;
    const float col3 = -1.f;
    const float col4 = 0.f;
    const float col5 = 1.f;
    const float col6 = 2.f;
    const float col7 = 3.f;
    const float col8 = 4.f;
    const float col9 = 5.f;

    struct TestVertex
    {
        TestVertex(FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate)
        {
            XMStoreFloat3(&this->position, position);
            XMStoreFloat3(&this->normal, normal);
            XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
            XMStoreFloat2(&this->textureCoordinate2, textureCoordinate * 3);
            XMStoreUByte4(&this->blendIndices, XMVectorSet(0, 1, 2, 3));

            XMStoreFloat3(&this->tangent, g_XMZero);

            float u = XMVectorGetX(textureCoordinate) - 0.5f;
            float v = XMVectorGetY(textureCoordinate) - 0.5f;

            float d = 1 - sqrt(u * u + v * v) * 2;

            if (d < 0)
                d = 0;

            XMStoreFloat4(&this->blendWeight, XMVectorSet(d, 1 - d, u, v));

            color = 0xFFFF00FF;
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

    using VertexCollection = std::vector<TestVertex>;
    using IndexCollection = std::vector<uint16_t>;


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
void Game::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

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

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Time-based animation
    float time = static_cast<float>(m_timer.GetTotalSeconds());

    float alphaFade = (sin(time * 2) + 1) / 2;

    if (alphaFade >= 1)
        alphaFade = 1 - FLT_EPSILON;

    float yaw = time * 0.4f;
    float pitch = time * 0.7f;
    float roll = time * 1.1f;

    XMVECTORF32 blue;
#ifdef GAMMA_CORRECT_RENDERING
    blue.v = XMColorSRGBToRGB(Colors::Blue);
#else
    blue.v = Colors::Blue;
#endif

    XMMATRIX world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

    // Setup for teapot drawing.
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //--- BasicEFfect ----------------------------------------------------------------------
    // Simple unlit teapot.
    m_basicEffectUnlit->SetAlpha(1.f);
    m_basicEffectUnlit->SetWorld(world * XMMatrixTranslation(col0, row0, 0));
    m_basicEffectUnlit->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Unlit with alpha fading.
    m_basicEffectUnlit->SetAlpha(alphaFade);
    m_basicEffectUnlit->SetWorld(world * XMMatrixTranslation(col0, row1, 0));
    m_basicEffectUnlit->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Unlit with fog.
    m_basicEffectUnlitFog->SetWorld(world * XMMatrixTranslation(col0, row2, 2 - alphaFade * 6));
    m_basicEffectUnlitFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple unlit teapot with vertex colors.
    m_basicEffectUnlitVc->SetAlpha(1.f);
    m_basicEffectUnlitVc->SetWorld(world * XMMatrixTranslation(colA, row0, 0));
    m_basicEffectUnlitVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple unlit teapot with alpha fading with vertex colors.
    m_basicEffectUnlitVc->SetAlpha(alphaFade);
    m_basicEffectUnlitVc->SetWorld(world * XMMatrixTranslation(colA, row1, 0));
    m_basicEffectUnlitVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Unlit with fog with vertex colors.
    m_basicEffectUnlitFogVc->SetWorld(world * XMMatrixTranslation(colA, row2, 2 - alphaFade * 6));
    m_basicEffectUnlitFogVc->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit teapot.
    m_basicEffect->SetAlpha(1.f);
    m_basicEffect->SetWorld(world * XMMatrixTranslation(col1, row0, 0));
    m_basicEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit teapot, no specular.
    m_basicEffectNoSpecular->SetAlpha(1.f);
    m_basicEffectNoSpecular->SetWorld(world * XMMatrixTranslation(col2, row3, 0));
    m_basicEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit with alpha fading.
    m_basicEffect->SetAlpha(alphaFade);
    m_basicEffect->SetWorld(world * XMMatrixTranslation(col1, row1, 0));
    m_basicEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit alpha fading, no specular.
    m_basicEffectNoSpecular->SetAlpha(alphaFade);
    m_basicEffectNoSpecular->SetWorld(world * XMMatrixTranslation(col3, row3, 0));
    m_basicEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Simple lit with fog.
    m_basicEffectFog->SetAlpha(1.f);
    m_basicEffectFog->SetWorld(world * XMMatrixTranslation(col1, row2, 2 - alphaFade * 6));
    m_basicEffectFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    {
        // Light only from above.
        m_basicEffect->SetAlpha(1.f);
        m_basicEffect->SetLightDirection(0, XMVectorSet(0, -1, 0, 0));
        m_basicEffect->SetLightEnabled(1, false);
        m_basicEffect->SetLightEnabled(2, false);
        m_basicEffect->SetWorld(world *  XMMatrixTranslation(col2, row0, 0));
        m_basicEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from the left.
        m_basicEffect->SetLightDirection(0, XMVectorSet(1, 0, 0, 0));
        m_basicEffect->SetWorld(world * XMMatrixTranslation(col3, row0, 0));
        m_basicEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from straight in front.
        m_basicEffect->SetLightDirection(0, XMVectorSet(0, 0, -1, 0));
        m_basicEffect->SetWorld(world * XMMatrixTranslation(col4, row0, 0));
        m_basicEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }

    m_basicEffect->EnableDefaultLighting();

    // Non uniform scaling.
    m_basicEffect->SetWorld(XMMatrixScaling(1, 2, 0.25f) * world * XMMatrixTranslation(col5, row0, 0));
    m_basicEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    {
        // Light only from above + per pixel lighting.
        m_basicEffectPPL->SetLightDirection(0, XMVectorSet(0, -1, 0, 0));
        m_basicEffectPPL->SetLightEnabled(1, false);
        m_basicEffectPPL->SetLightEnabled(2, false);
        m_basicEffectPPL->SetWorld(world *  XMMatrixTranslation(col2, row1, 0));
        m_basicEffectPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from the left + per pixel lighting.
        m_basicEffectPPL->SetLightDirection(0, XMVectorSet(1, 0, 0, 0));
        m_basicEffectPPL->SetWorld(world * XMMatrixTranslation(col3, row1, 0));
        m_basicEffectPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from straight in front + per pixel lighting.
        m_basicEffectPPL->SetLightDirection(0, XMVectorSet(0, 0, -1, 0));
        m_basicEffectPPL->SetWorld(world * XMMatrixTranslation(col4, row1, 0));
        m_basicEffectPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        m_basicEffectPPL->EnableDefaultLighting();

        // Non uniform scaling + per pixel lighting.
        m_basicEffectPPL->SetWorld(XMMatrixScaling(1, 2, 0.25f) * world * XMMatrixTranslation(col5, row1, 0));
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
    m_skinnedEffectNoSpecular->SetWorld(world * XMMatrixTranslation(col5, row3, 0));
    m_skinnedEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Skinned effect with fog.
    m_skinnedEffectFog->SetBoneTransforms(bones, 4);
    m_skinnedEffectFog->SetWorld(world * XMMatrixTranslation(colA, rowA, 2 - alphaFade * 6));
    m_skinnedEffectFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_skinnedEffectFogPPL->SetBoneTransforms(bones, 4);
    m_skinnedEffectFogPPL->SetWorld(world * XMMatrixTranslation(col0, rowA, 2 - alphaFade * 6));
    m_skinnedEffectFogPPL->Apply(commandList);
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
    m_skinnedEffect->SetWorld(world * XMMatrixTranslation(col3, rowA, 0));
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

    m_skinnedEffectPPL->SetBoneTransforms(bones, 4);
    m_skinnedEffectPPL->SetWorld(world * XMMatrixTranslation(col1, rowA, 0));
    m_skinnedEffectPPL->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- EnvironmentMapEffect -------------------------------------------------------------
    m_envmap->SetEnvironmentMapAmount(1.f);
    m_envmap->SetFresnelFactor(1.f);
    m_envmap->SetWorld(world * XMMatrixTranslation(col6, row0, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map with alpha fading.
    m_envmap->SetAlpha(alphaFade);
    m_envmap->SetWorld(world * XMMatrixTranslation(col6, row1, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    m_envmap->SetAlpha(1.f);
 
    // Environment map with fog.
    m_envmapFog->SetWorld(world * XMMatrixTranslation(col6, row2, 2 - alphaFade * 6));
    m_envmapFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map, animating the fresnel factor.
    m_envmap->SetFresnelFactor(alphaFade * 3);
    m_envmap->SetWorld(world * XMMatrixTranslation(col6, row4, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map, animating the amount, with no fresnel.
    m_envmapNoFresnel->SetEnvironmentMapAmount(alphaFade);
    m_envmapNoFresnel->SetWorld(world * XMMatrixTranslation(col6, row5, 0));
    m_envmapNoFresnel->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map, animating the amount.
    m_envmap->SetEnvironmentMapAmount(alphaFade);
    m_envmap->SetWorld(world * XMMatrixTranslation(col6, row6, 0));
    m_envmap->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Environment map, with animating specular
    m_envmapSpec->SetEnvironmentMapSpecular(blue * alphaFade);
    m_envmapSpec->SetWorld(world * XMMatrixTranslation(col5, row5, 0));
    m_envmapSpec->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    {
        // Light only from above + per pixel lighting, animating the fresnel factor.
        m_envmapPPL->SetLightDirection(0, XMVectorSet(0, -1, 0, 0));
        m_envmapPPL->SetLightEnabled(1, false);
        m_envmapPPL->SetLightEnabled(2, false);
        m_envmapPPL->SetFresnelFactor(alphaFade * 3);
        m_envmapPPL->SetWorld(world *  XMMatrixTranslation(col7, row4, 0));
        m_envmapPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from the left + per pixel lighting, animating the amount, with no fresnel.
        m_envmapNoFresnelPPL->SetEnvironmentMapAmount(alphaFade);
        m_envmapNoFresnelPPL->SetFresnelFactor(0);
        m_envmapNoFresnelPPL->SetLightDirection(0, XMVectorSet(1, 0, 0, 0));
        m_envmapNoFresnelPPL->SetLightEnabled(1, false);
        m_envmapNoFresnelPPL->SetLightEnabled(2, false);
        m_envmapNoFresnelPPL->SetWorld(world * XMMatrixTranslation(col7, row5, 0));
        m_envmapNoFresnelPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from straight in front + per pixel lighting with fog.
        m_envmapFogPPL->SetLightDirection(0, XMVectorSet(0, 0, -1, 0));
        m_envmapFogPPL->SetLightEnabled(1, false);
        m_envmapFogPPL->SetLightEnabled(2, false);
        m_envmapFogPPL->SetWorld(world * XMMatrixTranslation(col7, row6, 2 - alphaFade * 6));
        m_envmapFogPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Light only from the left + per pixel lighting, with animating specular 
        m_envmapSpecPPL->SetLightDirection(0, XMVectorSet(1, 0, 0, 0));
        m_envmapSpecPPL->SetLightEnabled(1, false);
        m_envmapSpecPPL->SetLightEnabled(2, false);
        m_envmapSpecPPL->SetEnvironmentMapSpecular(blue * alphaFade);
        m_envmapSpecPPL->SetWorld(world * XMMatrixTranslation(col5, row6, 0));
        m_envmapSpecPPL->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }

    //--- DualTextureEFfect ----------------------------------------------------------------
    m_dualTexture->SetAlpha(1.f);
    m_dualTexture->SetWorld(world *  XMMatrixTranslation(col7, row0, 0));
    m_dualTexture->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Dual texture with alpha fading.
    m_dualTexture->SetAlpha(alphaFade);
    m_dualTexture->SetWorld(world *  XMMatrixTranslation(col7, row1, 0));
    m_dualTexture->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Dual texture with fog.
    m_dualTextureFog->SetWorld(world *  XMMatrixTranslation(col7, row2, 2 - alphaFade * 6));
    m_dualTextureFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- AlphaTestEFfect ------------------------------------------------------------------

    {
        // Alpha test, > 0.
        m_alphaTest->SetReferenceAlpha(0);
        m_alphaTest->SetWorld(world * XMMatrixTranslation(col8, row0, 0));
        m_alphaTest->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, > 128.
        m_alphaTest->SetReferenceAlpha(128);
        m_alphaTest->SetWorld(world * XMMatrixTranslation(col8, row1, 0));
        m_alphaTest->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test with fog.
        m_alphaTestFog->SetReferenceAlpha(128);
        m_alphaTestFog->SetWorld(world * XMMatrixTranslation(col8, row2, 2 - alphaFade * 6));
        m_alphaTestFog->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, < animating value.
        m_alphaTestLess->SetReferenceAlpha(1 + (int)(alphaFade * 254));
        m_alphaTestLess->SetWorld(world * XMMatrixTranslation(col8, row4, 0));
        m_alphaTestLess->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, = 255.
        m_alphaTestEqual->SetReferenceAlpha(255);
        m_alphaTestEqual->SetWorld(world * XMMatrixTranslation(col8, row5, 0));
        m_alphaTestEqual->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        // Alpha test, != 0.
        m_alphaTestNotEqual->SetReferenceAlpha(0);
        m_alphaTestNotEqual->SetWorld(world * XMMatrixTranslation(col8, row6, 0));
        m_alphaTestNotEqual->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }

    //--- NormalMapEffect ------------------------------------------------------------------
    m_normalMapEffect->SetAlpha(1.f);
    m_normalMapEffect->SetWorld(world *  XMMatrixTranslation(col9, row0, 0));
    m_normalMapEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // NormalMapEffect no spec
    m_normalMapEffectNoSpecular->SetWorld(world *  XMMatrixTranslation(col9, row1, 0));
    m_normalMapEffectNoSpecular->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // NormalMap with fog.
    m_normalMapEffectFog->SetWorld(world *  XMMatrixTranslation(col9, row2, 2 - alphaFade * 6));
    m_normalMapEffectFog->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // NormalMap with default diffuse
    m_normalMapEffectNoDiffuse->SetWorld(world *  XMMatrixTranslation(col9, row4, 0));
    m_normalMapEffectNoDiffuse->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // NormalMap with default diffuse no spec
    m_normalMapEffectNormalsOnly->SetWorld(world *  XMMatrixTranslation(col9, row5, 0));
    m_normalMapEffectNormalsOnly->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

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

    XMVECTORF32 red, blue, gray;
#ifdef GAMMA_CORRECT_RENDERING
    red.v = XMColorSRGBToRGB(Colors::Red);
    blue.v = XMColorSRGBToRGB(Colors::Blue);
    gray.v = XMColorSRGBToRGB(Colors::Gray);
#else
    red.v = Colors::Red;
    blue.v = Colors::Blue;
    gray.v = Colors::Gray;
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
        m_basicEffectUnlit->SetDiffuseColor(blue);

        m_basicEffectUnlitFog = std::make_unique<BasicEffect>(device, EffectFlags::Fog, pdAlpha);
        m_basicEffectUnlitFog->SetDiffuseColor(blue);
        m_basicEffectUnlitFog->SetFogColor(gray);
        m_basicEffectUnlitFog->SetFogStart(fogstart);
        m_basicEffectUnlitFog->SetFogEnd(fogend);

        m_basicEffectUnlitVc = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pdAlpha);

        m_basicEffectUnlitFogVc = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor | EffectFlags::Fog, pdAlpha);
        m_basicEffectUnlitFogVc->SetFogColor(gray);
        m_basicEffectUnlitFogVc->SetFogStart(fogstart);
        m_basicEffectUnlitFogVc->SetFogEnd(fogend);

        m_basicEffect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pdAlpha);
        m_basicEffect->EnableDefaultLighting();
        m_basicEffect->SetDiffuseColor(red);

        m_basicEffectFog = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Fog, pdAlpha);
        m_basicEffectFog->EnableDefaultLighting();
        m_basicEffectFog->SetDiffuseColor(red);
        m_basicEffectFog->SetFogColor(gray);
        m_basicEffectFog->SetFogStart(fogstart);
        m_basicEffectFog->SetFogEnd(fogend);

        m_basicEffectNoSpecular = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pdAlpha);
        m_basicEffectNoSpecular->EnableDefaultLighting();
        m_basicEffectNoSpecular->SetDiffuseColor(red);
        m_basicEffectNoSpecular->DisableSpecular();

        m_basicEffectPPL = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting, pdAlpha);
        m_basicEffectPPL->EnableDefaultLighting();
        m_basicEffectPPL->SetDiffuseColor(red);

        //--- SkinnedEFfect ----------------------------------------------------------------
        m_skinnedEffect = std::make_unique<SkinnedEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pdAlpha);
        m_skinnedEffect->EnableDefaultLighting();

        m_skinnedEffectFog = std::make_unique<SkinnedEffect>(device, EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::Fog, pdAlpha);
        m_skinnedEffectFog->EnableDefaultLighting();
        m_skinnedEffectFog->SetFogColor(gray);
        m_skinnedEffectFog->SetFogStart(fogstart);
        m_skinnedEffectFog->SetFogEnd(fogend);

        m_skinnedEffectNoSpecular = std::make_unique<SkinnedEffect>(device, EffectFlags::Lighting, pdAlpha);
        m_skinnedEffectNoSpecular->EnableDefaultLighting();
        m_skinnedEffectNoSpecular->DisableSpecular();

        m_skinnedEffectPPL = std::make_unique<SkinnedEffect>(device, EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::PerPixelLighting, pdAlpha);
        m_skinnedEffectPPL->EnableDefaultLighting();

        m_skinnedEffectFogPPL = std::make_unique<SkinnedEffect>(device, EffectFlags::Lighting | EffectFlags::Texture | EffectFlags::PerPixelLighting | EffectFlags::Fog, pdAlpha);
        m_skinnedEffectFogPPL->EnableDefaultLighting();
        m_skinnedEffectFogPPL->SetFogColor(gray);
        m_skinnedEffectFogPPL->SetFogStart(fogstart);
        m_skinnedEffectFogPPL->SetFogEnd(fogend);

        //--- EnvironmentMapEffect ---------------------------------------------------------
        m_envmap = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pdAlpha);
        m_envmap->EnableDefaultLighting();

        m_envmapSpec = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pdAlpha, true, true);
        m_envmapSpec->EnableDefaultLighting();

        m_envmapFog = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Fog, pdAlpha);
        m_envmapFog->EnableDefaultLighting();
        m_envmapFog->SetFogColor(gray);
        m_envmapFog->SetFogStart(fogstart);
        m_envmapFog->SetFogEnd(fogend);

        m_envmapNoFresnel = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pdAlpha, false);
        m_envmapNoFresnel->EnableDefaultLighting();

        m_envmapPPL = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting, pdAlpha);
        m_envmapPPL->EnableDefaultLighting();

        m_envmapSpecPPL = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting, pdAlpha, true, true);
        m_envmapSpecPPL->EnableDefaultLighting();

        m_envmapFogPPL = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Fog | EffectFlags::PerPixelLighting, pdAlpha);
        m_envmapFogPPL->EnableDefaultLighting();
        m_envmapFogPPL->SetFogColor(gray);
        m_envmapFogPPL->SetFogStart(fogstart);
        m_envmapFogPPL->SetFogEnd(fogend);

        m_envmapNoFresnelPPL = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::PerPixelLighting, pdAlpha, false);
        m_envmapNoFresnelPPL->EnableDefaultLighting();

        //--- DualTextureEFfect ------------------------------------------------------------
        m_dualTexture = std::make_unique<DualTextureEffect>(device, EffectFlags::None, pdAlpha);

        m_dualTextureFog = std::make_unique<DualTextureEffect>(device, EffectFlags::Fog, pdAlpha);
        m_dualTextureFog->SetFogColor(gray);
        m_dualTextureFog->SetFogStart(fogstart);
        m_dualTextureFog->SetFogEnd(fogend);

        //--- AlphaTestEFfect --------------------------------------------------------------
        m_alphaTest = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque);

        m_alphaTestFog = std::make_unique<AlphaTestEffect>(device, EffectFlags::Fog, pdOpaque);
        m_alphaTestFog->SetFogColor(red);
        m_alphaTestFog->SetFogStart(fogstart);
        m_alphaTestFog->SetFogEnd(fogend);

        m_alphaTestLess = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque, D3D12_COMPARISON_FUNC_LESS);

        m_alphaTestEqual = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque, D3D12_COMPARISON_FUNC_EQUAL);

        m_alphaTestNotEqual = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pdOpaque, D3D12_COMPARISON_FUNC_NOT_EQUAL);

        //--- NormalMapEffect --------------------------------------------------------------
        m_normalMapEffect = std::make_unique<NormalMapEffect>(device, EffectFlags::None, pdOpaque);
        m_normalMapEffect->EnableDefaultLighting();
        m_normalMapEffect->SetDiffuseColor(Colors::White);

        m_normalMapEffectFog = std::make_unique<NormalMapEffect>(device, EffectFlags::Fog, pdOpaque);
        m_normalMapEffectFog->SetFogColor(gray);
        m_normalMapEffectFog->SetFogStart(fogstart);
        m_normalMapEffectFog->SetFogEnd(fogend);
        m_normalMapEffectFog->SetDiffuseColor(Colors::White);
        m_normalMapEffectFog->EnableDefaultLighting();

        m_normalMapEffectNoDiffuse = std::make_unique<NormalMapEffect>(device, EffectFlags::None, pdOpaque);
        m_normalMapEffectNoDiffuse->SetDiffuseColor(Colors::White);
        m_normalMapEffectNoDiffuse->EnableDefaultLighting();

        m_normalMapEffectNormalsOnly = std::make_unique<NormalMapEffect>(device, EffectFlags::None, pdOpaque, false);
        m_normalMapEffectNormalsOnly->SetDiffuseColor(Colors::White);
        m_normalMapEffectNormalsOnly->EnableDefaultLighting();
        m_normalMapEffectNormalsOnly->DisableSpecular();

        m_normalMapEffectNoSpecular = std::make_unique<NormalMapEffect>(device, EffectFlags::None, pdOpaque, false);
        m_normalMapEffectNoSpecular->SetDiffuseColor(Colors::White);
        m_normalMapEffectNoSpecular->EnableDefaultLighting();
        m_normalMapEffectNoSpecular->DisableSpecular();
    }

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

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"opaqueCat.dds",
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_opaqueCat.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_opaqueCat.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::OpaqueCat));

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

    // Set textures.
    auto opaqueCat = m_resourceDescriptors->GetGpuHandle(Descriptors::OpaqueCat);

    m_skinnedEffect->SetTexture(opaqueCat, m_states->LinearWrap());
    m_skinnedEffectFog->SetTexture(opaqueCat, m_states->LinearWrap());
    m_skinnedEffectNoSpecular->SetTexture(opaqueCat, m_states->LinearWrap());
    m_skinnedEffectPPL->SetTexture(opaqueCat, m_states->LinearWrap());
    m_skinnedEffectFogPPL->SetTexture(opaqueCat, m_states->LinearWrap());

    auto cubemap = m_resourceDescriptors->GetGpuHandle(Descriptors::Cubemap);

    m_envmap->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmap->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());
    m_envmapSpec->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmapSpec->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());
    m_envmapFog->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmapFog->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());
    m_envmapNoFresnel->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmapNoFresnel->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());
    m_envmapPPL->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmapPPL->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());
    m_envmapSpecPPL->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmapSpecPPL->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());
    m_envmapFogPPL->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmapFogPPL->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());
    m_envmapNoFresnelPPL->SetTexture(opaqueCat, m_states->LinearWrap());
    m_envmapNoFresnelPPL->SetEnvironmentMap(cubemap, m_states->AnisotropicWrap());

    auto overlay = m_resourceDescriptors->GetGpuHandle(Descriptors::Overlay);

    m_dualTexture->SetTexture(opaqueCat, m_states->LinearWrap());
    m_dualTexture->SetTexture2(overlay, m_states->LinearWrap());
    m_dualTextureFog->SetTexture(opaqueCat, m_states->LinearWrap());
    m_dualTextureFog->SetTexture2(overlay, m_states->LinearWrap());

    auto cat = m_resourceDescriptors->GetGpuHandle(Descriptors::Cat);

    m_alphaTest->SetTexture(cat, m_states->LinearWrap());
    m_alphaTestFog->SetTexture(cat, m_states->LinearWrap());
    m_alphaTestLess->SetTexture(cat, m_states->LinearWrap());
    m_alphaTestEqual->SetTexture(cat, m_states->LinearWrap());
    m_alphaTestNotEqual->SetTexture(cat, m_states->LinearWrap());

    {
        auto albeto = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickDiffuse);
        auto normal = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickNormal);
        auto specular = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickSpecular);
        auto defaultTex = m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex);

        auto sampler = m_states->LinearWrap();

        m_normalMapEffect->SetTexture(albeto, sampler);
        m_normalMapEffect->SetNormalTexture(normal);
        m_normalMapEffect->SetSpecularTexture(specular);

        m_normalMapEffectFog->SetTexture(albeto, sampler);
        m_normalMapEffectFog->SetNormalTexture(normal);
        m_normalMapEffectFog->SetSpecularTexture(specular);

        m_normalMapEffectNoDiffuse->SetTexture(defaultTex, sampler);
        m_normalMapEffectNoDiffuse->SetNormalTexture(normal);
        m_normalMapEffectNoDiffuse->SetSpecularTexture(specular);

        m_normalMapEffectNormalsOnly->SetTexture(defaultTex, sampler);
        m_normalMapEffectNormalsOnly->SetNormalTexture(normal);

        m_normalMapEffectNoSpecular->SetTexture(albeto, sampler);
        m_normalMapEffectNoSpecular->SetNormalTexture(normal);
    }
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

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    XMMATRIX orient = XMLoadFloat4x4(&m_deviceResources->GetOrientationTransform3D());
    projection *= orient;
#endif

    m_basicEffectUnlit->SetView(view);
    m_basicEffectUnlitFog->SetView(view);
    m_basicEffectUnlitVc->SetView(view);
    m_basicEffectUnlitFogVc->SetView(view);
    m_basicEffect->SetView(view);
    m_basicEffectFog->SetView(view);
    m_basicEffectNoSpecular->SetView(view);
    m_basicEffectPPL->SetView(view);

    m_skinnedEffect->SetView(view);
    m_skinnedEffectFog->SetView(view);
    m_skinnedEffectNoSpecular->SetView(view);
    m_skinnedEffectPPL->SetView(view);
    m_skinnedEffectFogPPL->SetView(view);

    m_envmap->SetView(view);
    m_envmapSpec->SetView(view);
    m_envmapFog->SetView(view);
    m_envmapNoFresnel->SetView(view);
    m_envmapPPL->SetView(view);
    m_envmapSpecPPL->SetView(view);
    m_envmapFogPPL->SetView(view);
    m_envmapNoFresnelPPL->SetView(view);

    m_dualTexture->SetView(view);
    m_dualTextureFog->SetView(view);

    m_alphaTest->SetView(view);
    m_alphaTestFog->SetView(view);
    m_alphaTestLess->SetView(view);
    m_alphaTestEqual->SetView(view);
    m_alphaTestNotEqual->SetView(view);

    m_normalMapEffect->SetView(view);
    m_normalMapEffectFog->SetView(view);
    m_normalMapEffectNoDiffuse->SetView(view);
    m_normalMapEffectNormalsOnly->SetView(view);
    m_normalMapEffectNoSpecular->SetView(view);

    m_basicEffectUnlit->SetProjection(projection);
    m_basicEffectUnlitFog->SetProjection(projection);
    m_basicEffectUnlitVc->SetProjection(projection);
    m_basicEffectUnlitFogVc->SetProjection(projection);
    m_basicEffect->SetProjection(projection);
    m_basicEffectFog->SetProjection(projection);
    m_basicEffectNoSpecular->SetProjection(projection);
    m_basicEffectPPL->SetProjection(projection);

    m_skinnedEffect->SetProjection(projection);
    m_skinnedEffectFog->SetProjection(projection);
    m_skinnedEffectNoSpecular->SetProjection(projection);
    m_skinnedEffectPPL->SetProjection(projection);
    m_skinnedEffectFogPPL->SetProjection(projection);

    m_envmap->SetProjection(projection);
    m_envmapSpec->SetProjection(projection);
    m_envmapFog->SetProjection(projection);
    m_envmapNoFresnel->SetProjection(projection);
    m_envmapPPL->SetProjection(projection);
    m_envmapSpecPPL->SetProjection(projection);
    m_envmapFogPPL->SetProjection(projection);
    m_envmapNoFresnelPPL->SetProjection(projection);

    m_dualTexture->SetProjection(projection);
    m_dualTextureFog->SetProjection(projection);

    m_alphaTest->SetProjection(projection);
    m_alphaTestFog->SetProjection(projection);
    m_alphaTestLess->SetProjection(projection);
    m_alphaTestEqual->SetProjection(projection);
    m_alphaTestNotEqual->SetProjection(projection);

    m_normalMapEffect->SetProjection(projection);
    m_normalMapEffectFog->SetProjection(projection);
    m_normalMapEffectNoDiffuse->SetProjection(projection);
    m_normalMapEffectNormalsOnly->SetProjection(projection);
    m_normalMapEffectNoSpecular->SetProjection(projection);
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_basicEffectUnlit.reset();
    m_basicEffectUnlitFog.reset();
    m_basicEffectUnlitVc.reset();
    m_basicEffectUnlitFogVc.reset();
    m_basicEffect.reset();
    m_basicEffectFog.reset();
    m_basicEffectNoSpecular.reset();
    m_basicEffectPPL.reset();

    m_skinnedEffect.reset();
    m_skinnedEffectFog.reset();
    m_skinnedEffectNoSpecular.reset();
    m_skinnedEffectPPL.reset();
    m_skinnedEffectFogPPL.reset();

    m_envmap.reset();
    m_envmapSpec.reset();
    m_envmapFog.reset();
    m_envmapNoFresnel.reset();
    m_envmapPPL.reset();
    m_envmapSpecPPL.reset();
    m_envmapFogPPL.reset();
    m_envmapNoFresnelPPL.reset();

    m_dualTexture.reset();
    m_dualTextureFog.reset();

    m_alphaTest.reset();
    m_alphaTestFog.reset();
    m_alphaTestLess.reset();
    m_alphaTestEqual.reset();
    m_alphaTestNotEqual.reset();

    m_normalMapEffect.reset();
    m_normalMapEffectFog.reset();
    m_normalMapEffectNoDiffuse.reset();
    m_normalMapEffectNormalsOnly.reset();
    m_normalMapEffectNoSpecular.reset();

    m_cat.Reset();
    m_opaqueCat.Reset();
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

    // Compute tangents
    ComputeTangents(indices, vertices);

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
