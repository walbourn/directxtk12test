//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK PBREffect
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"
#include "vbo.h"

// Build for LH vs. RH coords
//#define LH_COORDS

// For UWP/PC, this tests using a linear F16 swapchain intead of HDR10
//#define TEST_HDR_LINEAR

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

namespace
{
    constexpr float col0 = -4.25f;
    constexpr float col1 = -3.f;
    constexpr float col2 = -1.75f;
    constexpr float col3 = -.6f;
    constexpr float col4 = .6f;
    constexpr float col5 = 1.75f;
    constexpr float col6 = 3.f;
    constexpr float col7 = 4.25f;

    constexpr float row0 = 2.f;
    constexpr float row1 = 0.25f;
    constexpr float row2 = -1.1f;
    constexpr float row3 = -2.5f;

    void ReadVBO(_In_z_ const wchar_t* name, GeometricPrimitive::VertexCollection& vertices, GeometricPrimitive::IndexCollection& indices)
    {
        std::vector<uint8_t> blob;
        {
            std::ifstream inFile(name, std::ios::in | std::ios::binary | std::ios::ate);

            if (!inFile)
                throw std::runtime_error("ReadVBO");

            std::streampos len = inFile.tellg();
            if (!inFile)
                throw std::runtime_error("ReadVBO");

            if (len < static_cast<std::streampos>(sizeof(VBO::header_t)))
                throw std::runtime_error("ReadVBO");

            blob.resize(size_t(len));

            inFile.seekg(0, std::ios::beg);
            if (!inFile)
                throw std::runtime_error("ReadVBO");

            inFile.read(reinterpret_cast<char*>(blob.data()), len);
            if (!inFile)
                throw std::runtime_error("ReadVBO");

            inFile.close();
        }

        auto hdr = reinterpret_cast<const VBO::header_t*>(blob.data());

        if (!hdr->numIndices || !hdr->numVertices)
            throw std::runtime_error("ReadVBO");

        static_assert(sizeof(VertexPositionNormalTexture) == 32, "VBO vertex size mismatch");

        size_t vertSize = sizeof(VertexPositionNormalTexture) * hdr->numVertices;
        if (blob.size() < (vertSize + sizeof(VBO::header_t)))
            throw std::runtime_error("End of file");

        size_t indexSize = sizeof(uint16_t) * hdr->numIndices;
        if (blob.size() < (sizeof(VBO::header_t) + vertSize + indexSize))
            throw std::runtime_error("End of file");

        vertices.resize(hdr->numVertices);
        auto verts = reinterpret_cast<const VertexPositionNormalTexture*>(blob.data() + sizeof(VBO::header_t));
        memcpy_s(vertices.data(), vertices.size() * sizeof(VertexPositionNormalTexture), verts, vertSize);

        indices.resize(hdr->numIndices);
        auto tris = reinterpret_cast<const uint16_t*>(blob.data() + sizeof(VBO::header_t) + vertSize);
        memcpy_s(indices.data(), indices.size() * sizeof(uint16_t), tris, indexSize);
    }
}

Game::Game() noexcept(false) :
    m_indexCount(0),
    m_vertexBufferView{},
    m_indexBufferView{},
    m_indexCountCube(0),
    m_vertexBufferViewCube{},
    m_indexBufferViewCube{},
    m_ibl(0),
    m_spinning(true),
    m_showDebug(false),
    m_debugMode(DebugEffect::Mode_Default),
    m_pitch(0),
    m_yaw(0)
{
#if defined(TEST_HDR_LINEAR) && !defined(XBOX)
    const DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
#else
    const DXGI_FORMAT c_DisplayFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
#endif

#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD | DX::DeviceResources::c_EnableHDR
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_EnableHDR | DX::DeviceResources::c_Enable4K_Xbox);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_DisplayFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_EnableHDR);
#endif

#ifdef LOSTDEVICE
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

        if (m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED)
        {
            CycleDebug();
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

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Tab))
    {
        CycleDebug();
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
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    // Time-based animation
    float time = static_cast<float>(m_timer.GetTotalSeconds());

    float alphaFade = (sin(time * 2) + 1) / 2;

    if (alphaFade >= 1)
        alphaFade = 1 - FLT_EPSILON;

    float yaw = time * 0.4f;
    float pitch = time * 0.7f;
    float roll = time * 1.1f;

    XMMATRIX world;

    if (m_spinning)
    {
        world = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    }
    else
    {
        world = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0);
    }

    // Setup for teapot drawing.
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto radianceTex = m_resourceDescriptors->GetGpuHandle(Descriptors::RadianceIBL1 + size_t(m_ibl));

    auto diffuseDesc = m_radianceIBL[0]->GetDesc();

    auto irradianceTex = m_resourceDescriptors->GetGpuHandle(Descriptors::IrradianceIBL1 + size_t(m_ibl));
    m_pbr->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->AnisotropicClamp());
    m_pbrConstant->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->LinearWrap());
    m_pbrEmissive->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->LinearWrap());
    m_pbrCube->SetIBLTextures(radianceTex, diffuseDesc.MipLevels, irradianceTex, m_states->LinearWrap());

    if (m_showDebug)
    {
        //--- DebugEffect ------------------------------------------------------------------
        auto debug = m_debug.get();
        switch (m_debugMode)
        {
        case DebugEffect::Mode_Normals: debug = m_debugN.get(); break;
        case DebugEffect::Mode_Tangents: debug = m_debugT.get(); break;
        case DebugEffect::Mode_BiTangents: debug = m_debugB.get(); break;
        case DebugEffect::Mode_Default: break;
        }

        debug->SetAlpha(1.f);
        debug->SetWorld(world * XMMatrixTranslation(col0, row0, 0));
        debug->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        debug->SetWorld(world * XMMatrixTranslation(col3, row0, 0));
        debug->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        debug->SetWorld(world * XMMatrixTranslation(col1, row0, 0));
        debug->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        debug->SetAlpha(alphaFade);
        debug->SetWorld(world * XMMatrixTranslation(col2, row0, 0));
        debug->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViewCube);
        commandList->IASetIndexBuffer(&m_indexBufferViewCube);
        debug->SetAlpha(1.f);
        debug->SetWorld(world * XMMatrixTranslation(col7, row0, 0));
        debug->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCountCube, 1, 0, 0, 0);

        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);

        debug->SetAlpha(1.f);
        debug->SetWorld(world * XMMatrixTranslation(col4, row0, 0));
        debug->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        debug->SetAlpha(alphaFade);
        debug->SetWorld(world * XMMatrixTranslation(col5, row0, 0));
        debug->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    }
    else
    {
        auto albedoTex1 = m_resourceDescriptors->GetGpuHandle(Descriptors::BaseColor1);
        auto normalTex1 = m_resourceDescriptors->GetGpuHandle(Descriptors::NormalMap1);

        auto albedoTex2 = m_resourceDescriptors->GetGpuHandle(Descriptors::BaseColor2);
        auto normalTex2 = m_resourceDescriptors->GetGpuHandle(Descriptors::NormalMap2);

        //--- NormalMap --------------------------------------------------------------------
        m_normalMapEffect->SetWorld(world * XMMatrixTranslation(col0, row0, 0));
        m_normalMapEffect->SetTexture(albedoTex1, m_states->AnisotropicClamp());
        m_normalMapEffect->SetNormalTexture(normalTex1);
        m_normalMapEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        m_normalMapEffect->SetWorld(world * XMMatrixTranslation(col3, row0, 0));
        m_normalMapEffect->SetTexture(albedoTex2, m_states->AnisotropicClamp());
        m_normalMapEffect->SetNormalTexture(normalTex2);
        m_normalMapEffect->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        //--- PBREffect (basic) ------------------------------------------------------------
        {
            auto rmaTex1 = m_resourceDescriptors->GetGpuHandle(Descriptors::RMA1);

            m_pbr->SetAlpha(1.f);
            m_pbr->SetWorld(world * XMMatrixTranslation(col1, row0, 0));
            m_pbr->SetSurfaceTextures(albedoTex1, normalTex1, rmaTex1, m_states->AnisotropicClamp());
            m_pbr->Apply(commandList);
            commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

            m_pbr->SetAlpha(alphaFade);
            m_pbr->SetWorld(world * XMMatrixTranslation(col2, row0, 0));
            m_pbr->Apply(commandList);
            commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
        }

        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViewCube);
        commandList->IASetIndexBuffer(&m_indexBufferViewCube);
        m_pbrCube->SetWorld(world * XMMatrixTranslation(col7, row0, 0));
        m_pbrCube->Apply(commandList);
        commandList->DrawIndexedInstanced(m_indexCountCube, 1, 0, 0, 0);

        //--- PBREffect (emissive) ---------------------------------------------------------
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);

        {
            auto rmaTex2 = m_resourceDescriptors->GetGpuHandle(Descriptors::RMA2);
            auto emissiveTex = m_resourceDescriptors->GetGpuHandle(Descriptors::EmissiveTexture2);

            m_pbrEmissive->SetAlpha(1.f);
            m_pbrEmissive->SetWorld(world * XMMatrixTranslation(col4, row0, 0));
            m_pbrEmissive->SetSurfaceTextures(albedoTex2, normalTex2, rmaTex2, m_states->AnisotropicClamp());
            m_pbrEmissive->SetEmissiveTexture(emissiveTex);
            m_pbrEmissive->Apply(commandList);
            commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

            m_pbrEmissive->SetAlpha(alphaFade);
            m_pbrEmissive->SetWorld(world * XMMatrixTranslation(col5, row0, 0));
            m_pbrEmissive->Apply(commandList);
            commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
        }
    }

    //--- PBREffect (constant) -------------------------------------------------------------   
    m_pbrConstant->SetAlpha(1.f);
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
    m_pbrConstant->SetAlpha(alphaFade);
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
    m_pbrConstant->SetAlpha(1.f);
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

#ifdef XBOX
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

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

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
        &GeometricPrimitive::VertexType::InputLayout,
        CommonStates::AlphaBlend,
        CommonStates::DepthDefault,
#ifdef LH_COORDS
            CommonStates::CullClockwise,
#else
            CommonStates::CullCounterClockwise,
#endif
        hdrState);

    m_normalMapEffect = std::make_unique<NormalMapEffect>(device, EffectFlags::Texture, pipelineDesc);
    m_normalMapEffect->EnableDefaultLighting();

    m_pbrConstant = std::make_unique<PBREffect>(device, EffectFlags::None, pipelineDesc);
    m_pbrConstant->EnableDefaultLighting();

    m_pbr = std::make_unique<PBREffect>(device, EffectFlags::Texture, pipelineDesc);
    m_pbr->EnableDefaultLighting();

    m_pbrEmissive = std::make_unique<PBREffect>(device, EffectFlags::Texture | EffectFlags::Emissive, pipelineDesc);
    m_pbrEmissive->EnableDefaultLighting();

    m_pbrCube = std::make_unique<PBREffect>(device, EffectFlags::Texture | EffectFlags::Emissive, pipelineDesc);
    m_pbrCube->EnableDefaultLighting();

    m_debug = std::make_unique<DebugEffect>(device, EffectFlags::None, pipelineDesc);
    m_debugN = std::make_unique<DebugEffect>(device, EffectFlags::None, pipelineDesc, DebugEffect::Mode_Normals);
    m_debugT = std::make_unique<DebugEffect>(device, EffectFlags::None, pipelineDesc, DebugEffect::Mode_Tangents);
    m_debugB = std::make_unique<DebugEffect>(device, EffectFlags::None, pipelineDesc, DebugEffect::Mode_BiTangents);

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

    // Create test geometry
    {
        GeometricPrimitive::VertexCollection vertices;
        GeometricPrimitive::IndexCollection indices;

        GeometricPrimitive::CreateSphere(vertices, indices);

        // Create the D3D buffers.
        if (vertices.size() >= USHRT_MAX)
            throw std::runtime_error("Too many vertices for 16-bit index buffer");

        // Vertex data
        auto verts = reinterpret_cast<const uint8_t*>(vertices.data());
        size_t vertSizeBytes = vertices.size() * sizeof(GeometricPrimitive::VertexType);

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
        m_vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(GeometricPrimitive::VertexType));
        m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_vertexBuffer.Size());

        m_indexBufferView.BufferLocation = m_indexBuffer.GpuAddress();
        m_indexBufferView.SizeInBytes = static_cast<UINT>(m_indexBuffer.Size());
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    }

    // Create cube geometry
    {
        GeometricPrimitive::VertexCollection vertices;
        GeometricPrimitive::IndexCollection indices;

        ReadVBO(L"BrokenCube.vbo", vertices, indices);

        // Create the D3D buffers.
        if (vertices.size() >= USHRT_MAX)
            throw std::runtime_error("Too many vertices for 16-bit index buffer");

        // Vertex data
        auto verts = reinterpret_cast<const uint8_t*>(vertices.data());
        size_t vertSizeBytes = vertices.size() * sizeof(GeometricPrimitive::VertexType);

        m_vertexBufferCube = GraphicsMemory::Get().Allocate(vertSizeBytes);
        memcpy(m_vertexBufferCube.Memory(), verts, vertSizeBytes);

        // Index data
        auto ind = reinterpret_cast<const uint8_t*>(indices.data());
        size_t indSizeBytes = indices.size() * sizeof(uint16_t);

        m_indexBufferCube = GraphicsMemory::Get().Allocate(indSizeBytes);
        memcpy(m_indexBufferCube.Memory(), ind, indSizeBytes);

        // Record index count for draw
        m_indexCountCube = static_cast<UINT>(indices.size());

        // Create views
        m_vertexBufferViewCube.BufferLocation = m_vertexBufferCube.GpuAddress();
        m_vertexBufferViewCube.StrideInBytes = static_cast<UINT>(sizeof(GeometricPrimitive::VertexType));
        m_vertexBufferViewCube.SizeInBytes = static_cast<UINT>(m_vertexBufferCube.Size());

        m_indexBufferViewCube.BufferLocation = m_indexBufferCube.GpuAddress();
        m_indexBufferViewCube.SizeInBytes = static_cast<UINT>(m_indexBufferCube.Size());
        m_indexBufferViewCube.Format = DXGI_FORMAT_R16_UINT;
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // Load textures
    static const wchar_t* s_albedoTextures[s_nMaterials] =
    {
        L"SphereMat_baseColor.png",
        L"Sphere2Mat_baseColor.png",
        L"BrokenCube_baseColor.png",
    };
    static const wchar_t* s_normalMapTextures[s_nMaterials] =
    {
        L"SphereMat_normal.png",
        L"Sphere2Mat_normal.png",
        L"BrokenCube_normal.png",
    };
    static const wchar_t* s_rmaTextures[s_nMaterials] =
    {
        L"SphereMat_occlusionRoughnessMetallic.png",
        L"Sphere2Mat_occlusionRoughnessMetallic.png",
        L"BrokenCube_occlusionRoughnessMetallic.png",
    };
    static const wchar_t* s_emissiveTextures[s_nMaterials] =
    {
        nullptr,
        L"Sphere2Mat_emissive.png",
        L"BrokenCube_emissive.png",
    };

    static_assert(std::size(s_albedoTextures) == std::size(s_normalMapTextures), "Material array mismatch");
    static_assert(std::size(s_albedoTextures) == std::size(s_rmaTextures), "Material array mismatch");
    static_assert(std::size(s_albedoTextures) == std::size(s_emissiveTextures), "Material array mismatch");

    for (size_t j = 0; j < s_nMaterials; ++j)
    {
        DX::ThrowIfFailed(
            CreateWICTextureFromFile(device, resourceUpload, s_albedoTextures[j], m_baseColor[j].ReleaseAndGetAddressOf(), true)
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

        if (s_emissiveTextures[j])
        {
            DX::ThrowIfFailed(
                CreateWICTextureFromFile(device, resourceUpload, s_emissiveTextures[j], m_emissiveMap[j].ReleaseAndGetAddressOf(), true)
            );

            CreateShaderResourceView(device, m_emissiveMap[j].Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::EmissiveTexture1 + j), false);
        }
    }

    m_pbrCube->SetSurfaceTextures(
        m_resourceDescriptors->GetGpuHandle(Descriptors::BaseColor3),
        m_resourceDescriptors->GetGpuHandle(Descriptors::NormalMap3),
        m_resourceDescriptors->GetGpuHandle(Descriptors::RMA3),
        m_states->AnisotropicClamp());
    m_pbrCube->SetEmissiveTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::EmissiveTexture3));

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
    static const XMVECTORF32 cameraPosition = { { { 0.f, 0.f, 6.f, 0.f } } };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

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

    m_normalMapEffect->SetView(view);
    m_pbr->SetView(view);
    m_pbrConstant->SetView(view);
    m_pbrEmissive->SetView(view);
    m_pbrCube->SetView(view);
    m_debug->SetView(view);
    m_debugN->SetView(view);
    m_debugT->SetView(view);
    m_debugB->SetView(view);

    m_normalMapEffect->SetProjection(projection);
    m_pbr->SetProjection(projection);
    m_pbrConstant->SetProjection(projection);
    m_pbrEmissive->SetProjection(projection);
    m_pbrCube->SetProjection(projection);
    m_debug->SetProjection(projection);
    m_debugN->SetProjection(projection);
    m_debugT->SetProjection(projection);
    m_debugB->SetProjection(projection);

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
    for (size_t j = 0; j < s_nMaterials; ++j)
    {
        m_baseColor[j].Reset();
        m_normalMap[j].Reset();
        m_rma[j].Reset();
        m_emissiveMap[j].Reset();
    }

    for (size_t j = 0; j < s_nIBL; ++j)
    {
        m_radianceIBL[j].Reset();
        m_irradianceIBL[j].Reset();
    }

    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();

    m_indexBufferCube.Reset();
    m_vertexBufferCube.Reset();

    m_normalMapEffect.reset();
    m_pbr.reset();
    m_pbrConstant.reset();
    m_pbrCube.reset();
    m_debug.reset();
    m_debugN.reset();
    m_debugT.reset();
    m_debugB.reset();

    m_states.reset();

    m_toneMap.reset();
    m_toneMapLinear.reset();
    m_toneMapHDR10.reset();

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

void Game::CycleDebug()
{
    if (!m_showDebug)
    {
        m_showDebug = true;
        m_debugMode = DebugEffect::Mode_Default;
    }
    else if (m_debugMode == DebugEffect::Mode_BiTangents)
    {
        m_showDebug = false;
    }
    else
    {
        m_debugMode = static_cast<DebugEffect::Mode>(m_debugMode + 1);

        switch (m_debugMode)
        {
        case DebugEffect::Mode_Normals: OutputDebugStringA("INFO: Showing normals\n"); break;
        case DebugEffect::Mode_Tangents: OutputDebugStringA("INFO: Showing tangents\n"); break;
        case DebugEffect::Mode_BiTangents: OutputDebugStringA("INFO: Showing bi-tangents\n"); break;
        case DebugEffect::Mode_Default: break;
        }
    }
}
