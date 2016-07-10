//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

// Build for LH vs. RH coords
//#define LH_COORDS

namespace
{
    const float row0 = 2.7f;
    const float row1 = 1.f;
    const float row2 = -0.7f;
    const float row3 = -2.5f;

    const float col0 = -7.5f;
    const float col1 = -5.75f;
    const float col2 = -4.25f;
    const float col3 = -2.7f;
    const float col4 = -1.25f;
    const float col5 = 0.f;
    const float col6 = 1.25f;
    const float col7 = 2.5f;
    const float col8 = 4.25f;
    const float col9 = 5.75f;
    const float col10 = 7.5f;
}

using namespace DirectX;

using Microsoft::WRL::ComPtr;

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
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    elapsedTime;

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

    auto time = static_cast<float>(m_timer.GetTotalSeconds());

    float alphaFade = (sin(time * 2) + 1) / 2;

    if (alphaFade >= 1)
        alphaFade = 1 - FLT_EPSILON;

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
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(1, heaps);

    // Draw shapes.
    m_effect->SetWorld(world * XMMatrixTranslation(col0, row0, 0));
    m_effect->SetDiffuseColor(Colors::White);
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col1, row0, 0));
    m_effect->SetDiffuseColor(Colors::Red);
    m_effect->Apply(commandList);
    m_sphere->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col2, row0, 0));
    m_effect->SetDiffuseColor(Colors::Green);
    m_effect->Apply(commandList);
    m_geosphere->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col3, row0, 0));
    m_effect->SetDiffuseColor(Colors::Lime);
    m_effect->Apply(commandList);
    m_cylinder->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col4, row0, 0));
    m_effect->SetDiffuseColor(Colors::Yellow);
    m_effect->Apply(commandList);
    m_cone->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col5, row0, 0));
    m_effect->SetDiffuseColor(Colors::Blue);
    m_effect->Apply(commandList);
    m_torus->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col6, row0, 0));
    m_effect->SetDiffuseColor(Colors::CornflowerBlue);
    m_effect->Apply(commandList);
    m_teapot->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col7, row0, 0));
    m_effect->SetDiffuseColor(Colors::Red);
    m_effect->Apply(commandList);
    m_tetra->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col8, row0, 0));
    m_effect->SetDiffuseColor(Colors::Lime);
    m_effect->Apply(commandList);
    m_octa->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col9, row0, 0));
    m_effect->SetDiffuseColor(Colors::Blue);
    m_effect->Apply(commandList);
    m_dodec->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col10, row0, 0));
    m_effect->SetDiffuseColor(Colors::Cyan);
    m_effect->Apply(commandList);
    m_iso->Draw(commandList);

    m_effect->SetWorld(world * XMMatrixTranslation(col8, row3, 0));
    m_effect->SetDiffuseColor(Colors::Magenta);
    m_effect->Apply(commandList);
    m_box->Draw(commandList);

    // Draw textured shapes.
    m_effectTexture->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::RefTexture));
    m_effectTexture->SetWorld(world * XMMatrixTranslation(col0, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::White);
    m_effectTexture->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col1, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Red);
    m_effectTexture->Apply(commandList);
    m_sphere->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col2, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Green);
    m_effectTexture->Apply(commandList);
    m_geosphere->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col3, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Lime);
    m_effectTexture->Apply(commandList);
    m_cylinder->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col4, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Yellow);
    m_effectTexture->Apply(commandList);
    m_cone->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col5, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Blue);
    m_effectTexture->Apply(commandList);
    m_torus->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col6, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::CornflowerBlue);
    m_effectTexture->Apply(commandList);
    m_teapot->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col7, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Red);
    m_effectTexture->Apply(commandList);
    m_tetra->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col8, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Lime);
    m_effectTexture->Apply(commandList);
    m_octa->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col9, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Blue);
    m_effectTexture->Apply(commandList);
    m_dodec->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col10, row1, 0));
    m_effectTexture->SetDiffuseColor(Colors::Cyan);
    m_effectTexture->Apply(commandList);
    m_iso->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col9, row3, 0));
    m_effectTexture->SetDiffuseColor(Colors::Magenta);
    m_effectTexture->Apply(commandList);
    m_box->Draw(commandList);

    m_effectTexture->SetWorld(world * XMMatrixTranslation(col7, row3, 0));
    m_effectTexture->SetDiffuseColor(Colors::White);
    m_effectTexture->Apply(commandList);
    m_customBox->Draw(commandList);

    // Draw shapes in wireframe.
    m_effectWireframe->SetDiffuseColor(Colors::Gray);
    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col0, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col1, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_sphere->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col2, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_geosphere->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col3, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_cylinder->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col4, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_cone->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col5, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_torus->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col6, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_teapot->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col7, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_tetra->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col8, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_octa->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col9, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_dodec->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col10, row2, 0));
    m_effectWireframe->Apply(commandList);
    m_iso->Draw(commandList);

    m_effectWireframe->SetWorld(world * XMMatrixTranslation(col10, row3, 0));
    m_effectWireframe->Apply(commandList);
    m_box->Draw(commandList);

    // Draw shapes with alpha blending.
    m_effectAlpha->SetWorld(world * XMMatrixTranslation(col0, row3, 0));
    m_effectAlpha->SetDiffuseColor(Colors::White * alphaFade);
    m_effectAlpha->SetAlpha(alphaFade);
    m_effectAlpha->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectPMAlphaTexture->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat));
    m_effectPMAlphaTexture->SetWorld(world * XMMatrixTranslation(col1, row3, 0));
    m_effectPMAlphaTexture->SetDiffuseColor(Colors::White * alphaFade);
    m_effectPMAlphaTexture->SetAlpha(alphaFade);
    m_effectPMAlphaTexture->Apply(commandList);
    m_cube->Draw(commandList);

    m_effectAlphaTexture->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Cat));
    m_effectAlphaTexture->SetWorld(world * XMMatrixTranslation(col2, row3, 0));
    m_effectAlphaTexture->SetDiffuseColor(Colors::White * alphaFade);
    m_effectAlphaTexture->SetAlpha(alphaFade);
    m_effectAlphaTexture->Apply(commandList);
    m_cube->Draw(commandList);

    // Draw dynamic light.
    XMVECTOR quat = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
    XMVECTOR dir = XMVector3Rotate(g_XMOne, quat);
    m_effectLights->SetLightDirection(0, dir);
    m_effectLights->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo));
    m_effectLights->SetWorld(XMMatrixTranslation(col3, row3, 0));
    m_effectLights->Apply(commandList);
    m_cube->Draw(commandList);

    // Draw dynamic light and fog.
    XMMATRIX fbworld = XMMatrixTranslation(0, 0, cos(time) * 2.f);
    m_effectFog->SetWorld(fbworld  * XMMatrixTranslation(col5, row3, 0));
    m_effectFog->SetLightDirection(0, dir);
    m_effectFog->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo));
    m_effectFog->Apply(commandList);
    m_cube->Draw(commandList);

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
    width = 1600;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    // Create effects.
    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::Opaque,
            &CommonStates::DepthDefault,
            &CommonStates::CullCounterClockwise,
            &rtState);

        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting, pd);
        m_effect->EnableDefaultLighting();
    }

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::Opaque,
            &CommonStates::DepthDefault,
            &CommonStates::Wireframe,
            &rtState);

        m_effectWireframe = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting, pd);
        m_effectWireframe->EnableDefaultLighting();
    }

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::Opaque,
            &CommonStates::DepthDefault,
            &CommonStates::CullCounterClockwise,
            &rtState);

        m_effectTexture = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectTexture->EnableDefaultLighting();

    }

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::AlphaBlend,
            &CommonStates::DepthDefault,
            &CommonStates::CullCounterClockwise,
            &rtState);

        m_effectAlpha = std::make_unique<BasicEffect>(device, EffectFlags::PerPixelLighting, pd);
        m_effectAlpha->EnableDefaultLighting();
    }

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::AlphaBlend,
            &CommonStates::DepthDefault,
            &CommonStates::CullCounterClockwise,
            &rtState);

        m_effectPMAlphaTexture = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectPMAlphaTexture->EnableDefaultLighting();
    }

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::NonPremultiplied,
            &CommonStates::DepthDefault,
            &CommonStates::CullCounterClockwise,
            &rtState);

        m_effectAlphaTexture = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectAlphaTexture->EnableDefaultLighting();
    }

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::Opaque,
            &CommonStates::DepthDefault,
            &CommonStates::CullCounterClockwise,
            &rtState);

        m_effectLights = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::PerPixelLighting, pd);
        m_effectLights->EnableDefaultLighting();
    }

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            &CommonStates::Opaque,
            &CommonStates::DepthDefault,
            &CommonStates::CullCounterClockwise,
            &rtState);

        m_effectFog = std::make_unique<BasicEffect>(device, EffectFlags::Texture | EffectFlags::Fog | EffectFlags::PerPixelLighting, pd);
        m_effectFog->EnableDefaultLighting();

#ifdef LH_COORDS
        m_effectFog->SetFogStart(-6);
        m_effectFog->SetFogEnd(-8);
#else
        m_effectFog->SetFogStart(6);
        m_effectFog->SetFogEnd(8);
#endif
        m_effectFog->SetFogColor(Colors::CornflowerBlue);
    }

    // Create shapes.
#ifdef LH_COORDS
    bool rhcoords = false;
#else
    bool rhcoords = true;
#endif

    m_cube = GeometricPrimitive::CreateCube(1.f, rhcoords);
    m_box = GeometricPrimitive::CreateBox(XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f), rhcoords);
    m_sphere = GeometricPrimitive::CreateSphere(1.f, 16, rhcoords);
    m_geosphere = GeometricPrimitive::CreateGeoSphere(1.f, 3, rhcoords);
    m_cylinder = GeometricPrimitive::CreateCylinder(1.f, 1.f, 32, rhcoords);
    m_cone = GeometricPrimitive::CreateCone(1.f, 1.f, 32, rhcoords);
    m_torus = GeometricPrimitive::CreateTorus(1.f, 0.333f, 32, rhcoords);
    m_teapot = GeometricPrimitive::CreateTeapot(1.f, 8, rhcoords);
    m_tetra = GeometricPrimitive::CreateTetrahedron(0.75f, rhcoords);
    m_octa = GeometricPrimitive::CreateOctahedron(0.75f, rhcoords);
    m_dodec = GeometricPrimitive::CreateDodecahedron(0.5f, rhcoords);
    m_iso = GeometricPrimitive::CreateIcosahedron(0.5f, rhcoords);

    {
        std::vector<VertexPositionNormalTexture> customVerts;
        std::vector<uint16_t> customIndices;
        GeometricPrimitive::CreateBox(customVerts, customIndices, XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f), rhcoords);

        for (auto it = customVerts.begin(); it != customVerts.end(); ++it)
        {
            it->textureCoordinate.x *= 5.f;
            it->textureCoordinate.y *= 5.f;
        }

        m_customBox = GeometricPrimitive::CreateCustom(customVerts, customIndices);
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
        CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo.dds", m_dxLogo.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_dxLogo.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo));

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, resourceUpload, L"reftexture.dds", m_refTexture.ReleaseAndGetAddressOf()));

    CreateShaderResourceView(device, m_refTexture.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::RefTexture));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    m_deviceResources->WaitForGpu();

    uploadResourcesFinished.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 cameraPosition = { 0, 0, 7 };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 10);
#else
    XMMATRIX view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
    XMMATRIX projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 10);
#endif

    m_effect->SetView(view);
    m_effectWireframe->SetView(view);
    m_effectTexture->SetView(view);
    m_effectAlpha->SetView(view);
    m_effectPMAlphaTexture->SetView(view);
    m_effectAlphaTexture->SetView(view);
    m_effectLights->SetView(view);
    m_effectFog->SetView(view);
    
    m_effect->SetProjection(projection);
    m_effectWireframe->SetProjection(projection);
    m_effectTexture->SetProjection(projection);
    m_effectAlpha->SetProjection(projection);
    m_effectPMAlphaTexture->SetProjection(projection);
    m_effectAlphaTexture->SetProjection(projection);
    m_effectLights->SetProjection(projection);
    m_effectFog->SetProjection(projection);
}

void Game::OnDeviceLost()
{
    m_cube.reset();
    m_box.reset();
    m_sphere.reset();
    m_geosphere.reset();
    m_cylinder.reset();
    m_cone.reset();
    m_torus.reset();
    m_teapot.reset();
    m_tetra.reset();
    m_octa.reset();
    m_dodec.reset();
    m_iso.reset();
    m_customBox.reset();

    m_effect.reset();
    m_effectWireframe.reset();
    m_effectTexture.reset();
    m_effectAlpha.reset();
    m_effectPMAlphaTexture.reset();
    m_effectAlphaTexture.reset();
    m_effectLights.reset();
    m_effectFog.reset();

    m_cat.Reset();
    m_dxLogo.Reset();
    m_refTexture.Reset();

    m_resourceDescriptors.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
