//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK PBREffect
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

#pragma warning( disable : 4238 )

// Build for LH vs. RH coords
//#define LH_COORDS

// For UWP/PC, this tests using a linear F16 swapchain intead of HDR10
//#define TEST_HDR_LINEAR

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const float col0 = -4.25f;
    const float col1 = -3.f;
    const float col2 = -1.75f;
    const float col3 = -.6f;
    const float col4 = .6f;
    const float col5 = 1.75f;
    const float col6 = 3.f;
    const float col7 = 4.25f;

    const float row0 = 2.f;
    const float row1 = 0.25f;
    const float row2 = -1.1f;
    const float row3 = -2.5f;

    struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

    // Helper for computing tangents (see DirectXMesh <http://go.microsoft.com/fwlink/?LinkID=324981>)
    void ComputeTangents(const std::vector<uint16_t>& indices, std::vector<VertexPositionNormalTextureTangent>& vertices)
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
}

Game::Game() :
    m_ibl(0)
{
#if defined(_XBOX_ONE) && defined(_TITLE)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD
        | DX::DeviceResources::c_EnableHDR);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(
#if defined(TEST_HDR_LINEAR)
        DXGI_FORMAT_R16G16B16A16_FLOAT,
#else
        DXGI_FORMAT_R10G10B10A2_UNORM,
#endif
        DXGI_FORMAT_D32_FLOAT, 2,
        D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_EnableHDR);
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_deviceResources->RegisterDeviceNotify(this);
#endif

    // Set up for HDR rendering.
    m_hdrScene = std::make_unique<DX::RenderTexture>(DXGI_FORMAT_R16G16B16A16_FLOAT);

    XMVECTORF32 color;
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
    m_hdrScene->SetClearColor(color);
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
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitGame();
        }

        if (m_gamePadButtons.dpadDown == GamePad::ButtonStateTracker::PRESSED)
        {
            ++m_ibl;
            if (m_ibl >= s_nIBL)
            {
                m_ibl = 0;
            }
        }
        else if (m_gamePadButtons.dpadUp == GamePad::ButtonStateTracker::PRESSED)
        {
            if (!m_ibl)
                m_ibl = s_nIBL - 1;
            else
                --m_ibl;
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitGame();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
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

    auto commandList = m_deviceResources->GetCommandList();
    m_hdrScene->BeginScene(commandList);

    Clear();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

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

    XMMATRIX world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

    XMMATRIX worldX2 = world * XMMatrixScaling(2.f, 2.f, 2.f);

    // Setup for teapot drawing.
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto radianceTex = m_resourceDescriptors->GetGpuHandle(Descriptors::RadianceIBL1 + m_ibl);

    auto diffuseDesc = m_radianceIBL[0]->GetDesc();

    auto irradianceTex = m_resourceDescriptors->GetGpuHandle(Descriptors::IrradianceIBL1 + m_ibl);
    m_pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
    m_pbrConstant->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->LinearWrap());
    m_pbrEmissive->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->LinearWrap());

    auto albetoTex1 = m_resourceDescriptors->GetGpuHandle(Descriptors::BaseColor1);
    auto normalTex1 = m_resourceDescriptors->GetGpuHandle(Descriptors::NormalMap1);

    auto albetoTex2 = m_resourceDescriptors->GetGpuHandle(Descriptors::BaseColor2);
    auto normalTex2 = m_resourceDescriptors->GetGpuHandle(Descriptors::NormalMap2);

    //--- NormalMap ------------------------------------------------------------------------
    m_normalMapEffect->SetWorld(worldX2 * XMMatrixTranslation(col0, row0, 0));
    m_normalMapEffect->SetTexture(albetoTex1, m_states->AnisotropicClamp());
    m_normalMapEffect->SetNormalTexture(normalTex1);
    m_normalMapEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_normalMapEffect->SetWorld(worldX2 * XMMatrixTranslation(col4, row0, 0));
    m_normalMapEffect->SetTexture(albetoTex2, m_states->AnisotropicClamp());
    m_normalMapEffect->SetNormalTexture(normalTex2);
    m_normalMapEffect->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    //--- PBREffect (basic) ----------------------------------------------------------------
    {
        auto rmaTex1 = m_resourceDescriptors->GetGpuHandle(Descriptors::RMA1);

        m_pbr->SetWorld(worldX2 * XMMatrixTranslation(col2, row0, 0));
        m_pbr->SetSurfaceTextures(albetoTex1, normalTex1, rmaTex1, m_states->AnisotropicClamp());
        m_pbr->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }   

    {
        auto rmaTex2 = m_resourceDescriptors->GetGpuHandle(Descriptors::RMA2);
        auto emissiveTex = m_resourceDescriptors->GetGpuHandle(Descriptors::EmissiveTexture);

        m_pbrEmissive->SetWorld(worldX2 * XMMatrixTranslation(col6, row0, 0));
        m_pbrEmissive->SetSurfaceTextures(albetoTex2, normalTex2, rmaTex2, m_states->AnisotropicClamp());
        m_pbrEmissive->SetEmissiveTexture(emissiveTex);
        m_pbrEmissive->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }   

    //--- PBREffect (constant) -------------------------------------------------------------   
    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col0, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Blue);
    m_pbrConstant->SetConstantMetallic(1.f);
    m_pbrConstant->SetConstantRoughness(0.2f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col1, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Green);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col2, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Red);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col3, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Yellow);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col4, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Cyan);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col5, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Magenta);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col6, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Black);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col7, row1, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::White);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Row 2
    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col0, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Blue);
    m_pbrConstant->SetConstantMetallic(0.f);
    m_pbrConstant->SetConstantRoughness(0.2f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col1, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Green);
    m_pbrConstant->SetConstantMetallic(0.25f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col2, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Red);
    m_pbrConstant->SetConstantMetallic(0.5f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col3, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Yellow);
    m_pbrConstant->SetConstantMetallic(0.75f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col4, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Cyan);
    m_pbrConstant->SetConstantMetallic(1.f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col5, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Magenta);
    m_pbrConstant->SetConstantMetallic(0.5f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col6, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Black);
    m_pbrConstant->SetConstantMetallic(0.75f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col7, row2, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::White);
    m_pbrConstant->SetConstantMetallic(0.8f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Row3
    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col0, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Blue);
    m_pbrConstant->SetConstantMetallic(0.5f);
    m_pbrConstant->SetConstantRoughness(0.0f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col1, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Green);
    m_pbrConstant->SetConstantRoughness(0.25f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col2, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Red);
    m_pbrConstant->SetConstantRoughness(0.5f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col3, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Yellow);
    m_pbrConstant->SetConstantRoughness(0.75f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col4, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Cyan);
    m_pbrConstant->SetConstantRoughness(1.0f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col5, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Magenta);
    m_pbrConstant->SetConstantRoughness(0.2f);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col6, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::Black);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    m_pbrConstant->SetWorld(world * XMMatrixTranslation(col7, row3, 0));
    m_pbrConstant->SetConstantAlbedo(Colors::White);
    m_pbrConstant->Apply(commandList);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    PIXEndEvent(commandList);

    // Tonemap the frame.
    m_hdrScene->EndScene(commandList);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Tonemap");

#if defined(_XBOX_ONE) && defined(_TITLE)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptors[2] = { m_deviceResources->GetRenderTargetView(), m_deviceResources->GetGameDVRRenderTargetView() };
    commandList->OMSetRenderTargets(2, rtvDescriptors, FALSE, nullptr);

    m_toneMap->Process(commandList);
#else
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
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
    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene);
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    XMVECTORF32 color;
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
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

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_states = std::make_unique<CommonStates>(device);

    m_hdrScene->SetDevice(device,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::HDRScene));

    RenderTargetState hdrState(m_hdrScene->GetFormat(), m_deviceResources->GetDepthBufferFormat());

    EffectPipelineStateDescription pipelineDesc(
        &VertexPositionNormalTextureTangent::InputLayout,
        CommonStates::AlphaBlend,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        hdrState);

    m_normalMapEffect = std::make_unique<NormalMapEffect>(device, EffectFlags::Texture, pipelineDesc, false);
    m_normalMapEffect->EnableDefaultLighting();

    m_pbrConstant = std::make_unique<PBREffect>(device, EffectFlags::None, pipelineDesc);
    m_pbrConstant->EnableDefaultLighting();

    m_pbr = std::make_unique<PBREffect>(device, EffectFlags::Texture, pipelineDesc);
    m_pbr->EnableDefaultLighting();

    m_pbrEmissive = std::make_unique<PBREffect>(device, EffectFlags::Texture, pipelineDesc, true);
    m_pbrEmissive->EnableDefaultLighting();

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
#if defined(_XBOX_ONE) && defined(_TITLE)
    rtState.numRenderTargets = 2;
    rtState.rtvFormats[1] = m_deviceResources->GetGameDVRFormat();
#endif

    m_toneMap = std::make_unique<ToneMapPostProcess>(
        device,
        rtState,
        ToneMapPostProcess::ACESFilmic, (m_deviceResources->GetBackBufferFormat() == DXGI_FORMAT_R16G16B16A16_FLOAT) ? ToneMapPostProcess::Linear : ToneMapPostProcess::SRGB
#if defined(_XBOX_ONE) && defined(_TITLE)
        , true
#endif
        );

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::Linear);
    m_toneMapHDR10 = std::make_unique<ToneMapPostProcess>(device, rtState, ToneMapPostProcess::None, ToneMapPostProcess::ST2084);
#endif

    // Create test geometry with tangent frame
    {
        std::vector<GeometricPrimitive::VertexType> origVerts;
        std::vector<uint16_t> indices;

        GeometricPrimitive::CreateSphere(origVerts, indices);

        std::vector<VertexPositionNormalTextureTangent> vertices;
        vertices.reserve(origVerts.size());

        for (auto it = origVerts.cbegin(); it != origVerts.cend(); ++it)
        {
            VertexPositionNormalTextureTangent v;
            v.position = it->position;
            v.normal = it->normal;
            v.textureCoordinate = it->textureCoordinate;
            vertices.emplace_back(v);
        }

        assert(origVerts.size() == vertices.size());

        ComputeTangents(indices, vertices);

        // Create the D3D buffers.
        if (vertices.size() >= USHRT_MAX)
            throw std::exception("Too many vertices for 16-bit index buffer");

        // Vertex data
        auto verts = reinterpret_cast<const uint8_t*>(vertices.data());
        size_t vertSizeBytes = vertices.size() * sizeof(VertexPositionNormalTextureTangent);

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
        m_vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(VertexPositionNormalTextureTangent));
        m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexBuffer.Size());

        m_indexBufferView.BufferLocation = m_indexBuffer.GpuAddress();
        m_indexBufferView.SizeInBytes = static_cast<UINT>(m_indexBuffer.Size());
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // Load textures
    static const wchar_t* s_albetoTextures[s_nMaterials] =
    {
        L"SphereMat_baseColor.png",
        L"Sphere2Mat_baseColor.png"
    };
    static const wchar_t* s_normalMapTextures[s_nMaterials] =
    {
        L"SphereMat_normal.png",
        L"Sphere2Mat_normal.png"
    };
    static const wchar_t* s_rmaTextures[s_nMaterials] =
    {
        L"SphereMat_occlusionRoughnessMetallic.png",
        L"Sphere2Mat_occlusionRoughnessMetallic.png"
    };

    for (size_t j = 0; j < s_nMaterials; ++j)
    {
        DX::ThrowIfFailed(
            CreateWICTextureFromFile(device, resourceUpload, s_albetoTextures[j], m_baseColor[j].ReleaseAndGetAddressOf(), true)
        );

        DX::ThrowIfFailed(
            CreateWICTextureFromFile(device, resourceUpload, s_normalMapTextures[j], m_normalMap[j].ReleaseAndGetAddressOf(), true)
        );

        DX::ThrowIfFailed(
            CreateWICTextureFromFile(device, resourceUpload, s_rmaTextures[j], m_rma[j].ReleaseAndGetAddressOf(), true)
        );

        CreateShaderResourceView(device, m_baseColor[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BaseColor1 + j), false);
        CreateShaderResourceView(device, m_normalMap[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::NormalMap1 + j), false);
        CreateShaderResourceView(device, m_rma[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::RMA1 + j), false);
    }

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"Sphere2Mat_emissive.png", m_emissiveMap.ReleaseAndGetAddressOf(), true)
    );

    CreateShaderResourceView(device, m_emissiveMap.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::EmissiveTexture), false);

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

    for (size_t j = 0; j < s_nIBL; ++j)
    {
        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, s_radianceIBL[j], m_radianceIBL[j].ReleaseAndGetAddressOf())
        );

        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, s_irradianceIBL[j], m_irradianceIBL[j].ReleaseAndGetAddressOf())
        );

        CreateShaderResourceView(device, m_radianceIBL[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::RadianceIBL1 + j), true);
        CreateShaderResourceView(device, m_irradianceIBL[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::IrradianceIBL1 + j), true);
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();
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

    m_normalMapEffect->SetView(view);
    m_pbr->SetView(view);
    m_pbrConstant->SetView(view);
    m_pbrEmissive->SetView(view);

    m_normalMapEffect->SetProjection(projection);
    m_pbr->SetProjection(projection);
    m_pbrConstant->SetProjection(projection);
    m_pbrEmissive->SetProjection(projection);

    // Set windows size for HDR.
    m_hdrScene->SetWindow(size);

    m_toneMap->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
    m_toneMapHDR10->SetHDRSourceTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::SceneTex));
#endif
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    for (size_t j = 0; j < s_nMaterials; ++j)
    {
        m_baseColor[j].Reset();
        m_normalMap[j].Reset();
        m_rma[j].Reset();
    }

    m_emissiveMap.Reset();

    for (size_t j = 0; j < s_nIBL; ++j)
    {
        m_radianceIBL[j].Reset();
        m_irradianceIBL[j].Reset();
    }

    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();

    m_normalMapEffect.reset();
    m_pbr.reset();
    m_pbrConstant.reset();

    m_states.reset();

    m_toneMap.reset();

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_toneMapLinear.reset();
    m_toneMapHDR10.reset();
#endif

    m_hdrScene->ReleaseDevice();

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
