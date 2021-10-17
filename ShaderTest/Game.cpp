//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK - HLSL shader coverage
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#define GAMMA_CORRECT_RENDERING

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float SWAP_TIME = 1.f;
    constexpr float INTERACTIVE_TIME = 10.f;

    constexpr float ortho_width = 6.f;
    constexpr float ortho_height = 6.f;

    struct TestVertex
    {
        TestVertex(FXMVECTOR position, FXMVECTOR normal, FXMVECTOR textureCoordinate, uint32_t color)
        {
            XMStoreFloat3(&this->position, position);
            XMStoreFloat3(&this->normal, normal);
            XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
            XMStoreFloat2(&this->textureCoordinate2, XMVectorScale(textureCoordinate, 3.f));
            XMStoreUByte4(&this->blendIndices, XMVectorSet(0, 0, 0, 0));

            XMStoreFloat4(&this->blendWeight, XMVectorSet(1.f, 0.f, 0.f, 0.f));

            XMVECTOR clr = XMLoadUByteN4(reinterpret_cast<XMUBYTEN4*>(&color));
            XMStoreFloat4(&this->color, clr);
        }

        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT2 textureCoordinate;
        XMFLOAT2 textureCoordinate2;
        XMUBYTE4 blendIndices;
        XMFLOAT4 blendWeight;
        XMFLOAT4 color;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;
        static const D3D12_INPUT_LAYOUT_DESC InstancedInputLayout;

    private:
        static constexpr unsigned int InputElementCount = 7;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];

        static constexpr unsigned int InstancedInputElementCount = 10;
        static const D3D12_INPUT_ELEMENT_DESC InstancedInputElements[InstancedInputElementCount];
    };

    const D3D12_INPUT_ELEMENT_DESC TestVertex::InputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    const D3D12_INPUT_ELEMENT_DESC TestVertex::InstancedInputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",     1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "InstMatrix",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",   1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",   2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    const D3D12_INPUT_LAYOUT_DESC TestVertex::InputLayout =
    {
        TestVertex::InputElements,
        TestVertex::InputElementCount
    };
    
    const D3D12_INPUT_LAYOUT_DESC TestVertex::InstancedInputLayout =
    {
        TestVertex::InstancedInputElements,
        TestVertex::InstancedInputElementCount
    };

    using VertexCollection = std::vector<TestVertex>;
    using IndexCollection = std::vector<uint16_t>;

    struct TestCompressedVertex
    {
        TestCompressedVertex(const TestVertex& bn)
        {
            position = bn.position;
            blendIndices = bn.blendIndices;

            XMVECTOR v = XMLoadFloat3(&bn.normal);
            v = XMVectorMultiplyAdd(v, g_XMOneHalf, g_XMOneHalf);
            XMStoreFloat3PK(&this->normal, v);

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
        XMHALF2 textureCoordinate;
        XMHALF2 textureCoordinate2;
        XMUBYTE4 blendIndices;
        XMUBYTEN4 blendWeight;
        XMCOLOR color;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;
        static const D3D12_INPUT_LAYOUT_DESC InstancedInputLayout;

    private:
        static constexpr int InputElementCount = 7;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];

        static constexpr unsigned int InstancedInputElementCount = 10;
        static const D3D12_INPUT_ELEMENT_DESC InstancedInputElements[InstancedInputElementCount];
    };

    const D3D12_INPUT_ELEMENT_DESC TestCompressedVertex::InputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R11G11B10_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R16G16_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     1, DXGI_FORMAT_R16G16_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,   0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    const D3D12_INPUT_ELEMENT_DESC TestCompressedVertex::InstancedInputElements[] =
    {
        { "SV_Position",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "NORMAL",       0, DXGI_FORMAT_R11G11B10_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R16G16_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",     1, DXGI_FORMAT_R16G16_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "COLOR",        0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
        { "InstMatrix",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",   1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",   2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    const D3D12_INPUT_LAYOUT_DESC TestCompressedVertex::InputLayout =
    {
        TestCompressedVertex::InputElements,
        TestCompressedVertex::InputElementCount
    };

    const D3D12_INPUT_LAYOUT_DESC TestCompressedVertex::InstancedInputLayout =
    {
        TestCompressedVertex::InstancedInputElements,
        TestCompressedVertex::InstancedInputElementCount
    };
}  // anonymous namespace

Game::Game() noexcept(false) :
    m_indexCount(0),
    m_instanceCount(0),
    m_vertexBufferView{},
    m_vertexBufferViewBn{},
    m_indexBufferView{},
    m_renderMode(Render_Normal),
    m_delay(0)
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
        DX::DeviceResources::c_Enable4K_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
#endif

#ifdef LOSTDEVICE
    m_deviceResources->RegisterDeviceNotify(this);
#endif

    // Used for PBREffect velocity buffer
    m_velocityBuffer = std::make_unique<DX::RenderTexture>(DXGI_FORMAT_R32_UINT /*DXGI_FORMAT_R10G10B10A2_UNORM*/);
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
        CycleRenderMode();

        m_delay = INTERACTIVE_TIME;
    }
    else if (!kb.Space && !(pad.IsConnected() && pad.IsYPressed()))
    {
        m_delay -= static_cast<float>(timer.GetElapsedSeconds());

        if (m_delay <= 0.f)
        {
            CycleRenderMode();
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
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    // Setup for cube drawing.
    switch (m_renderMode)
    {
    case Render_Compressed:
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViewBn);
        break;

    case Render_Instanced:
    case Render_CompressedInstanced:
    {
        size_t j = 0;
        for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
        {
            XMMATRIX m = world * XMMatrixTranslation(x, 0.f, 0.f);
            XMStoreFloat3x4(&m_instanceTransforms[j], m);
            ++j;
        }

        assert(j == m_instanceCount);

        const size_t instBytes = j * sizeof(XMFLOAT3X4);

        GraphicsResource inst = m_graphicsMemory->Allocate(instBytes);
        memcpy(inst.Memory(), m_instanceTransforms.get(), instBytes);

        D3D12_VERTEX_BUFFER_VIEW vertexBuffers[2] = { (m_renderMode == Render_CompressedInstanced) ? m_vertexBufferViewBn : m_vertexBufferView };
        vertexBuffers[1].BufferLocation = inst.GpuAddress();
        vertexBuffers[1].SizeInBytes = static_cast<UINT>(instBytes);
        vertexBuffers[1].StrideInBytes = sizeof(XMFLOAT3X4);
        commandList->IASetVertexBuffers(0, 2, vertexBuffers);
    }
    break;

    default:
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        break;
    }

    commandList->IASetIndexBuffer(&m_indexBufferView);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    float y = ortho_height - 0.5f;
    if (m_renderMode == Render_Instanced || m_renderMode == Render_CompressedInstanced)
    {
        bool showCompressed = (m_renderMode == Render_CompressedInstanced);

        // NormalMapEffect (instanced)
        {
            auto it = (showCompressed) ? m_normalMapInstancedBn.cbegin() : m_normalMapInstanced.cbegin();
            auto eit = (showCompressed) ? m_normalMapInstancedBn.cend() : m_normalMapInstanced.cend();
            assert(it != eit);

            for (; y > -ortho_height; y -= 1.f)
            {
                (*it)->SetWorld(XMMatrixTranslation(0, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, m_instanceCount, 0, 0, 0);

                ++it;
                if (it == eit)
                    break;
            }

            // Make sure we drew all the effects
            assert(it == eit);

            y -= 1.f;
        }

        // PBREffect (instanced)
        {
            auto it = (showCompressed) ? m_pbrInstancedBn.cbegin() : m_pbrInstanced.cbegin();
            auto eit = (showCompressed) ? m_pbrInstancedBn.cend() : m_pbrInstanced.cend();
            assert(it != eit);

            for (; y > -ortho_height; y -= 1.f)
            {
                (*it)->SetWorld(XMMatrixTranslation(0, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, m_instanceCount, 0, 0, 0);

                ++it;
                if (it == eit)
                    break;
            }

            // Make sure we drew all the effects
            assert(it == eit);

            y -= 1.f;
        }

        // DebugEffect (instanced)
        {
            auto it = (showCompressed) ? m_debugInstancedBn.cbegin() : m_debugInstanced.cbegin();
            auto eit = (showCompressed) ? m_debugInstancedBn.cend() : m_debugInstanced.cend();
            assert(it != eit);

            for (; y > -ortho_height; y -= 1.f)
            {
                (*it)->SetWorld(XMMatrixTranslation(0, y, -1.f));
                (*it)->Apply(commandList);
                commandList->DrawIndexedInstanced(m_indexCount, m_instanceCount, 0, 0, 0);

                ++it;
                if (it == eit)
                    break;
            }

            // Make sure we drew all the effects
            assert(it == eit);

            y -= 1.f;
        }
    }
    else
    {
        bool showCompressed = (m_renderMode == Render_Compressed);

        // BasicEffect
        {
            auto it = (showCompressed) ? m_basicBn.cbegin() : m_basic.cbegin();
            auto eit = (showCompressed) ? m_basicBn.cend() : m_basic.cend();
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
            auto it = (showCompressed) ? m_skinningBn.cbegin() : m_skinning.cbegin();
            auto eit = (showCompressed) ? m_skinningBn.cend() : m_skinning.cend();
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
            auto it = (showCompressed) ? m_envmapBn.cbegin() : m_envmap.cbegin();
            auto eit = (showCompressed) ? m_envmapBn.cend() : m_envmap.cend();
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

        commandList->IASetVertexBuffers(0, 1, (showCompressed) ? &m_vertexBufferViewBn : &m_vertexBufferView);

        // NormalMapEffect
        float lastx = 0.f;
        {
            auto it = (showCompressed) ? m_normalMapBn.cbegin() : m_normalMap.cbegin();
            auto eit = (showCompressed) ? m_normalMapBn.cend() : m_normalMap.cend();
            assert(it != eit);

            for (; y > -ortho_height; y -= 1.f)
            {
                float x;
                for (x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
                {
                    (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                    (*it)->Apply(commandList);
                    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                    ++it;
                    if (it == eit)
                        break;
                }
                lastx = x;

                if (it == eit)
                    break;
            }

            // Make sure we drew all the effects
            assert(it == eit);

            // SkinnedNormalMapEffect should be on same line...
        }

        // SkinnedNormalMapEffect
        {
            auto it = (showCompressed) ? m_skinningNormalMapBn.cbegin() : m_skinningNormalMap.cbegin();
            auto eit = (showCompressed) ? m_skinningNormalMapBn.cend() : m_skinningNormalMap.cend();
            assert(it != eit);

            for (; y > -ortho_height; y -= 1.f)
            {
                for (float x = lastx + 1.f; x < ortho_width; x += 1.f)
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

        // PBREffect
        lastx = 0.f;
        {
            auto it = (showCompressed) ? m_pbrBn.cbegin() : m_pbr.cbegin();
            auto eit = (showCompressed) ? m_pbrBn.cend() : m_pbr.cend();
            assert(it != eit);

            for (; y > -ortho_height; y -= 1.f)
            {
                float x;
                for (x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
                {
                    (*it)->SetWorld(world * XMMatrixTranslation(x, y, -1.f));
                    (*it)->Apply(commandList);
                    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

                    ++it;
                    if (it == eit)
                        break;
                }
                lastx = x;

                if (it == eit)
                    break;
            }

            // Make sure we drew all the effects
            assert(it == eit);

            // SkinnedPBREffect should be on same line...
        }

        // SkinnedPBREffect
        {
            auto it = (showCompressed) ? m_skinningPbrBn.cbegin() : m_skinningPbr.cbegin();
            auto eit = (showCompressed) ? m_skinningPbrBn.cend() : m_skinningPbr.cend();
            assert(it != eit);

            for (; y > -ortho_height; y -= 1.f)
            {
                for (float x = lastx + 1.f; x < ortho_width; x += 1.f)
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

        // DebugEffect
        {
            auto it = (showCompressed) ? m_debugBn.cbegin() : m_debug.cbegin();
            auto eit = (showCompressed) ? m_debugBn.cend() : m_debug.cend();
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
    }

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
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    XMVECTORF32 color;
#ifdef GAMMA_CORRECT_RENDERING
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
#else
    color.v = Colors::CornflowerBlue;
#endif

    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptors[2] = { rtvDescriptor, m_renderDescriptors->GetCpuHandle(RTDescriptors::RTVelocityBuffer) };
    commandList->OMSetRenderTargets(2, rtvDescriptors, FALSE, &dsvDescriptor);
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

    m_states = std::make_unique<CommonStates>(device);

    CreateCube();

    // Load textures.
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

    m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_velocityBuffer->SetDevice(device,
        m_resourceDescriptors->GetCpuHandle(Descriptors::VelocityBuffer),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::RTVelocityBuffer));

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

#ifdef GAMMA_CORRECT_RENDERING
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_FORCE_SRGB;
    constexpr WIC_LOADER_FLAGS wicLoadFlags = WIC_LOADER_FORCE_SRGB;
#else
    constexpr DDS_LOADER_FLAGS loadFlags = DDS_LOADER_DEFAULT;
    constexpr WIC_LOADER_FLAGS wicLoadFlags = WIC_LOADER_DEFAULT;
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
        CreateWICTextureFromFileEx(device, resourceUpload, L"spheremap.bmp",
            0, D3D12_RESOURCE_FLAG_NONE, wicLoadFlags,
            m_envball.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_envball.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::SphereMap));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFileEx(device, resourceUpload, L"dualparabola.dds",
            0, D3D12_RESOURCE_FLAG_NONE, loadFlags,
            m_envdual.ReleaseAndGetAddressOf(), nullptr));

    CreateShaderResourceView(device, m_envdual.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DualParabolaMap));

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
        CreateDDSTextureFromFile(device, resourceUpload, L"spnza_bricks_a_normal.DDS",
            m_brickNormal.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickNormal.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickNormal));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"spnza_bricks_a_specular.DDS",
            m_brickSpecular.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_brickSpecular.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::BrickSpecular));

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"Sphere2Mat_baseColor.png",
            m_pbrAlbedo.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_pbrAlbedo.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::PBRAlbedo));

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"Sphere2Mat_normal.png",
            m_pbrNormal.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_pbrNormal.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::PBRNormal));

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"Sphere2Mat_occlusionRoughnessMetallic.png",
            m_pbrRMA.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_pbrRMA.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::PBR_RMA));

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"Sphere2Mat_emissive.png",
            m_pbrEmissive.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_pbrEmissive.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::PBREmissive));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"Atrium_diffuseIBL.dds",
            m_radianceIBL.ReleaseAndGetAddressOf())
    );

    CreateShaderResourceView(device, m_radianceIBL.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::RadianceIBL), true);

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"Atrium_specularIBL.dds",
            m_irradianceIBL.ReleaseAndGetAddressOf())
    );

    CreateShaderResourceView(device, m_irradianceIBL.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::IrradianceIBL), true);

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();

    // Create instance transforms.
    {
        size_t j = 0;
        for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
        {
            ++j;
        }
        m_instanceCount = static_cast<UINT>(j);

        m_instanceTransforms = std::make_unique<XMFLOAT3X4[]>(j);

        constexpr XMFLOAT3X4 s_identity = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f };

        j = 0;
        for (float x = -ortho_width + 0.5f; x < ortho_width; x += 1.f)
        {
            m_instanceTransforms[j] = s_identity;
            m_instanceTransforms[j]._14 = x;
            ++j;
        }
    }

    // Create test effects
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
    rtState.numRenderTargets = 2;
    rtState.rtvFormats[1] = m_velocityBuffer->GetFormat();

    auto sampler = m_states->LinearWrap();

    auto defaultTex = m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex);

    for (int j = 0; j < 2; ++j)
    {
        EffectPipelineStateDescription pd(
            (!j) ? &TestVertex::InputLayout : &TestCompressedVertex::InputLayout,
            CommonStates::AlphaBlend,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        EffectPipelineStateDescription opaquePd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);
        opaquePd.inputLayout = pd.inputLayout;

        EffectPipelineStateDescription pdInst(
            (!j) ? &TestVertex::InstancedInputLayout : &TestCompressedVertex::InstancedInputLayout,
            CommonStates::AlphaBlend,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        uint32_t eflags = (!j) ? EffectFlags::None : EffectFlags::BiasedVertexNormals;

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
            auto effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fresnel, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fog | EffectFlags::Fresnel, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect (no fresnel)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags, pd);
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

            // EnvironmentMapEffect (specular)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Specular, pd);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Specular | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect (fresnel + specular)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fresnel | EffectFlags::Specular, pd);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::Fresnel | EffectFlags::Specular | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect (per pixel lighting)
            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fresnel, pd, EnvironmentMapEffect::Mapping_Cube);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fresnel | EffectFlags::Fog, pd, EnvironmentMapEffect::Mapping_Cube);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, EnvironmentMapEffect::Mapping_Cube);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, EnvironmentMapEffect::Mapping_Cube);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Blue);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(envmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect sphere mapping (per pixel lighting only)
            auto spheremap = m_resourceDescriptors->GetGpuHandle(Descriptors::SphereMap);

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fresnel, pd, EnvironmentMapEffect::Mapping_Sphere);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Green);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(spheremap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fresnel | EffectFlags::Fog, pd, EnvironmentMapEffect::Mapping_Sphere);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Green);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(spheremap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, EnvironmentMapEffect::Mapping_Sphere);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Green);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(spheremap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, EnvironmentMapEffect::Mapping_Sphere);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Green);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(spheremap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            // EnvironmentMapEffect dual parabolic mapping (per pixel lighting only)
            auto dualmap = m_resourceDescriptors->GetGpuHandle(Descriptors::DualParabolaMap);

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fresnel, pd, EnvironmentMapEffect::Mapping_DualParabola);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Red);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(dualmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fresnel | EffectFlags::Fog, pd, EnvironmentMapEffect::Mapping_DualParabola);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Red);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(dualmap, sampler);
            effect->SetFogColor(Colors::Black);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting, pd, EnvironmentMapEffect::Mapping_DualParabola);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Red);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(dualmap, sampler);
            envmaps.emplace_back(std::move(effect));

            effect = std::make_unique<EnvironmentMapEffect>(device, eflags | EffectFlags::PerPixelLighting | EffectFlags::Fog, pd, EnvironmentMapEffect::Mapping_DualParabola);
            effect->EnableDefaultLighting();
            effect->SetEnvironmentMapSpecular(Colors::Red);
            effect->SetTexture(defaultTex, sampler);
            effect->SetEnvironmentMap(dualmap, sampler);
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
            std::vector<std::unique_ptr<DirectX::NormalMapEffect>> normalMapInst;

            // NormalMapEffect (no specular)
            auto effect = std::make_unique<NormalMapEffect>(device, eflags, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Instancing, pdInst);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            normalMapInst.emplace_back(std::move(effect));

            // NormalMapEffect (specular)
            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Specular, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Specular | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Instancing | EffectFlags::Specular, pdInst);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            normalMapInst.emplace_back(std::move(effect));

            // NormalMapEffect (vertex color)
            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Instancing | EffectFlags::VertexColor, pdInst);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            normalMapInst.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor | EffectFlags::Specular, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::Instancing | EffectFlags::VertexColor | EffectFlags::Specular, pdInst);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            normalMapInst.emplace_back(std::move(effect));

            effect = std::make_unique<NormalMapEffect>(device, eflags | EffectFlags::VertexColor | EffectFlags::Specular | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            if (!j)
            {
                m_normalMap.swap(normalMap);
                m_normalMapInstanced.swap(normalMapInst);
            }
            else
            {
                m_normalMapBn.swap(normalMap);
                m_normalMapInstancedBn.swap(normalMapInst);
            }
        }

        //-- SkinnedNormalMapEffect --------------------------------------------------------

        {
            auto diffuse = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickDiffuse);
            auto normal = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickNormal);
            auto specular = m_resourceDescriptors->GetGpuHandle(Descriptors::BrickSpecular);

            std::vector<std::unique_ptr<DirectX::SkinnedNormalMapEffect>> normalMap;

            // SkinnedNormalMapEffect (no specular)
            auto effect = std::make_unique<SkinnedNormalMapEffect>(device, eflags, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<SkinnedNormalMapEffect>(device, eflags | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            // SkinnedNormalMapEffect (specular)
            effect = std::make_unique<SkinnedNormalMapEffect>(device, eflags | EffectFlags::Specular, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            normalMap.emplace_back(std::move(effect));

            effect = std::make_unique<SkinnedNormalMapEffect>(device, eflags | EffectFlags::Specular | EffectFlags::Fog, pd);
            effect->EnableDefaultLighting();
            effect->SetTexture(diffuse, sampler);
            effect->SetNormalTexture(normal);
            effect->SetSpecularTexture(specular);
            effect->SetFogColor(Colors::Black);
            normalMap.emplace_back(std::move(effect));

            if (!j)
            {
                m_skinningNormalMap.swap(normalMap);
            }
            else
            {
                m_skinningNormalMapBn.swap(normalMap);
            }
        }

        //--- PBREffect --------------------------------------------------------------------

        {
            auto radiance = m_resourceDescriptors->GetGpuHandle(Descriptors::RadianceIBL);
            auto diffuseDesc = m_radianceIBL->GetDesc();
            auto irradiance = m_resourceDescriptors->GetGpuHandle(Descriptors::IrradianceIBL);

            std::vector<std::unique_ptr<DirectX::PBREffect>> pbr;
            std::vector<std::unique_ptr<DirectX::PBREffect>> pbrInst;

            // PBREffect
            auto effect = std::make_unique<PBREffect>(device, eflags, pd);
            effect->EnableDefaultLighting();
            effect->SetConstantAlbedo(Colors::Cyan);
            effect->SetConstantMetallic(0.5f);
            effect->SetConstantRoughness(0.75f);
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            pbr.emplace_back(std::move(effect));

            effect = std::make_unique<PBREffect>(device, eflags | EffectFlags::Instancing, pdInst);
            effect->EnableDefaultLighting();
            effect->SetConstantAlbedo(Colors::Cyan);
            effect->SetConstantMetallic(0.5f);
            effect->SetConstantRoughness(0.75f);
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            pbrInst.emplace_back(std::move(effect));

            // PBREffect (textured)
            auto albedo = m_resourceDescriptors->GetGpuHandle(Descriptors::PBRAlbedo);
            auto normal = m_resourceDescriptors->GetGpuHandle(Descriptors::PBRNormal);
            auto rma = m_resourceDescriptors->GetGpuHandle(Descriptors::PBR_RMA);

            effect = std::make_unique<PBREffect>(device, eflags | EffectFlags::Texture, pd);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            pbr.emplace_back(std::move(effect));

            effect = std::make_unique<PBREffect>(device, eflags | EffectFlags::Instancing | EffectFlags::Texture, pdInst);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            pbrInst.emplace_back(std::move(effect));

            // PBREffect (emissive)
            auto emissive = m_resourceDescriptors->GetGpuHandle(Descriptors::PBREmissive);

            effect = std::make_unique<PBREffect>(device, eflags | EffectFlags::Texture | EffectFlags::Emissive, pd);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            effect->SetEmissiveTexture(emissive);
            pbr.emplace_back(std::move(effect));

            effect = std::make_unique<PBREffect>(device, eflags | EffectFlags::Instancing | EffectFlags::Texture | EffectFlags::Emissive, pdInst);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            effect->SetEmissiveTexture(emissive);
            pbrInst.emplace_back(std::move(effect));

            // PBREffect (velocity)
            effect = std::make_unique<PBREffect>(device, eflags | EffectFlags::Texture | EffectFlags::Velocity, opaquePd);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            effect->SetRenderTargetSizeInPixels(1920, 1080);
            pbr.emplace_back(std::move(effect));

            // PBREffect (velocity + emissive)
            effect = std::make_unique<PBREffect>(device, eflags | EffectFlags::Texture | EffectFlags::Emissive | EffectFlags::Velocity, opaquePd);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            effect->SetEmissiveTexture(emissive);
            effect->SetRenderTargetSizeInPixels(1920, 1080);
            pbr.emplace_back(std::move(effect));

            if (!j)
            {
                m_pbr.swap(pbr);
                m_pbrInstanced.swap(pbrInst);
            }
            else
            {
                m_pbrBn.swap(pbr);
                m_pbrInstancedBn.swap(pbrInst);
            }
        }

        //--- SkinnedPBREffect -------------------------------------------------------------

        {
            auto radiance = m_resourceDescriptors->GetGpuHandle(Descriptors::RadianceIBL);
            auto diffuseDesc = m_radianceIBL->GetDesc();
            auto irradiance = m_resourceDescriptors->GetGpuHandle(Descriptors::IrradianceIBL);

            std::vector<std::unique_ptr<DirectX::SkinnedPBREffect>> pbr;

            // SkinnedPBREffect
            auto effect = std::make_unique<SkinnedPBREffect>(device, eflags, pd);
            effect->EnableDefaultLighting();
            effect->SetConstantAlbedo(Colors::Cyan);
            effect->SetConstantMetallic(0.5f);
            effect->SetConstantRoughness(0.75f);
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            pbr.emplace_back(std::move(effect));

            // SkinnedPBREffect (textured)
            auto albedo = m_resourceDescriptors->GetGpuHandle(Descriptors::PBRAlbedo);
            auto normal = m_resourceDescriptors->GetGpuHandle(Descriptors::PBRNormal);
            auto rma = m_resourceDescriptors->GetGpuHandle(Descriptors::PBR_RMA);

            effect = std::make_unique<SkinnedPBREffect>(device, eflags | EffectFlags::Texture, pd);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            pbr.emplace_back(std::move(effect));

            // SkinnedPBREffect (emissive)
            auto emissive = m_resourceDescriptors->GetGpuHandle(Descriptors::PBREmissive);

            effect = std::make_unique<SkinnedPBREffect>(device, eflags | EffectFlags::Texture | EffectFlags::Emissive, pd);
            effect->EnableDefaultLighting();
            effect->SetIBLTextures(radiance, diffuseDesc.MipLevels, irradiance, m_states->LinearWrap());
            effect->SetSurfaceTextures(albedo, normal, rma, m_states->AnisotropicClamp());
            effect->SetEmissiveTexture(emissive);
            pbr.emplace_back(std::move(effect));

            if (!j)
            {
                m_skinningPbr.swap(pbr);
            }
            else
            {
                m_skinningPbrBn.swap(pbr);
            }
        }

        //--- DebugEffect ------------------------------------------------------------------

        {
            std::vector<std::unique_ptr<DirectX::DebugEffect>> debug;
            std::vector<std::unique_ptr<DirectX::DebugEffect>> debugInst;

            // DebugEffect
            auto effect = std::make_unique<DebugEffect>(device, eflags, pd, DebugEffect::Mode_Default);
            debug.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags | EffectFlags::Instancing, pdInst, DebugEffect::Mode_Default);
            debugInst.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags, pd, DebugEffect::Mode_Normals);
            debug.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags, pd, DebugEffect::Mode_Tangents);
            debug.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags, pd, DebugEffect::Mode_BiTangents);
            debug.emplace_back(std::move(effect));

            // DebugEffect (vertex color)
            effect = std::make_unique<DebugEffect>(device, eflags | EffectFlags::VertexColor, pd, DebugEffect::Mode_Default);
            debug.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags | EffectFlags::Instancing | EffectFlags::VertexColor, pdInst, DebugEffect::Mode_Default);
            debugInst.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags | EffectFlags::VertexColor, pd, DebugEffect::Mode_Normals);
            debug.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags | EffectFlags::VertexColor, pd, DebugEffect::Mode_Tangents);
            debug.emplace_back(std::move(effect));

            effect = std::make_unique<DebugEffect>(device, eflags | EffectFlags::VertexColor, pd, DebugEffect::Mode_BiTangents);
            debug.emplace_back(std::move(effect));

            if (!j)
            {
                m_debug.swap(debug);
                m_debugInstanced.swap(debugInst);
            }
            else
            {
                m_debugBn.swap(debug);
                m_debugInstancedBn.swap(debugInst);
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
    auto size = m_deviceResources->GetOutputSize();
    m_velocityBuffer->SetWindow(size);

    XMMATRIX projection = XMMatrixOrthographicRH(ortho_width * 2.f, ortho_height * 2.f, 0.1f, 10.f);

#ifdef UWP
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        projection *= orient;
    }
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

    for (auto& it : m_skinningNormalMap)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_skinningNormalMapBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_pbr)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_pbrBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_skinningPbr)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_skinningPbrBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_debug)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_debugBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_normalMapInstanced)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_normalMapInstancedBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_pbrInstanced)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_pbrInstancedBn)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_debugInstanced)
    {
        it->SetProjection(projection);
    }

    for (auto& it : m_debugInstancedBn)
    {
        it->SetProjection(projection);
    }
}

#ifdef LOSTDEVICE
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
    m_skinningNormalMap.clear();
    m_skinningNormalMapBn.clear();
    m_pbr.clear();
    m_pbrBn.clear();
    m_skinningPbr.clear();
    m_skinningPbrBn.clear();
    m_debug.clear();
    m_debugBn.clear();

    m_normalMapInstanced.clear();
    m_normalMapInstancedBn.clear();
    m_pbrInstanced.clear();
    m_pbrInstancedBn.clear();
    m_debugInstanced.clear();
    m_debugInstancedBn.clear();

    m_cat.Reset();
    m_cubemap.Reset();
    m_envball.Reset();
    m_envdual.Reset();
    m_overlay.Reset();
    m_defaultTex.Reset();
    m_brickDiffuse.Reset();
    m_brickNormal.Reset();
    m_brickSpecular.Reset();
    m_pbrAlbedo.Reset();
    m_pbrNormal.Reset();
    m_pbrRMA.Reset();
    m_pbrEmissive.Reset();
    m_radianceIBL.Reset();
    m_irradianceIBL.Reset();

    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_vertexBufferBn.Reset();

    m_velocityBuffer->ReleaseDevice();

    m_resourceDescriptors.reset();
    m_renderDescriptors.reset();
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
    constexpr int FaceCount = 6;

    static const XMVECTORF32 faceNormals[FaceCount] =
    {
        { { { 0,  0,  1, 0 } } },
        { { { 0,  0, -1, 0 } } },
        { { { 1,  0,  0, 0 } } },
        { { { -1,  0,  0, 0 } } },
        { { { 0,  1,  0, 0 } } },
        { { { 0, -1,  0, 0 } } },
    };

    static const XMVECTORF32 textureCoordinates[4] =
    {
        { { { 1, 0, 0, 0 } } },
        { { { 1, 1, 0, 0 } } },
        { { { 0, 1, 0, 0 } } },
        { { { 0, 0, 0, 0 } } },
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

    const Vector4 tsize(0.25f, 0.25f, 0.25f, 0.f);

    // Create each face in turn.
    for (size_t i = 0; i < FaceCount; i++)
    {
        Vector4 normal = faceNormals[i].v;

        // Get two vectors perpendicular both to the face normal and to each other.
        XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

        Vector4 side1 = XMVector3Cross(normal, basis);
        Vector4 side2 = XMVector3Cross(normal, side1);

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

void Game::CycleRenderMode()
{
    m_renderMode += 1;

    if (m_renderMode >= Render_Max)
    {
        m_renderMode = Render_Normal;
    }
}
