//--------------------------------------------------------------------------------------
// File: effects.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "Effects.h"

#include "CommonStates.h"
#include "EffectPipelineStateDescription.h"
#include "RenderTargetState.h"
#include "VertexTypes.h"

#include <cstdio>
#include <type_traits>

#include <wrl/client.h>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_move_constructible<RenderTargetState>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<RenderTargetState>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<EffectPipelineStateDescription>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EffectPipelineStateDescription>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<BasicEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<BasicEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<AlphaTestEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<AlphaTestEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<DualTextureEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DualTextureEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<EnvironmentMapEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EnvironmentMapEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SkinnedEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SkinnedEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<NormalMapEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<NormalMapEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SkinnedNormalMapEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SkinnedNormalMapEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<PBREffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<PBREffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SkinnedPBREffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SkinnedPBREffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<DebugEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DebugEffect>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<EffectTextureFactory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EffectTextureFactory>::value, "Move Assign.");

// VS 2017 and the XDK isn't noexcept correct here
static_assert(std::is_nothrow_move_constructible<IEffectFactory::EffectInfo>::value, "Move Ctor.");
static_assert(std::is_move_assignable<IEffectFactory::EffectInfo>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<EffectFactory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EffectFactory>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<PBREffectFactory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<PBREffectFactory>::value, "Move Assign.");

namespace
{
    struct TestVertex
    {
        TestVertex(FXMVECTOR iposition, FXMVECTOR inormal, FXMVECTOR itextureCoordinate)
        {
            XMStoreFloat3(&this->position, iposition);
            XMStoreFloat3(&this->normal, inormal);
            XMStoreFloat2(&this->textureCoordinate, itextureCoordinate);
            XMStoreFloat2(&this->textureCoordinate2, XMVectorScale(itextureCoordinate, 3.f));
            PackedVector::XMStoreUByte4(&this->blendIndices, XMVectorSet(0, 1, 2, 3));

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
        XMFLOAT2 textureCoordinate2;
        PackedVector::XMUBYTE4 blendIndices;
        XMFLOAT4 blendWeight;
        PackedVector::XMUBYTE4 color;

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;

    private:
        static constexpr unsigned int InputElementCount = 7;
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
        { "COLOR",        0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    const D3D12_INPUT_LAYOUT_DESC TestVertex::InputLayout =
    {
        TestVertex::InputElements,
        TestVertex::InputElementCount
    };
}

_Success_(return)
bool Test05(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    bool success = true;

    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    EffectPipelineStateDescription pd(
        &VertexPositionNormalTexture::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

    std::unique_ptr<BasicEffect> basic;
    try
    {
        // TODO: Fog, VertexColor, Texture, Lighting, PerPixelLightingBit, BiasedVertexNormals
        basic = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        success = false;
    }

    std::unique_ptr<AlphaTestEffect> alpha;
    try
    {
        // TODO: Fog, VertexColor, D3D12_COMPARISON_FUNC_EQUAL
        alpha = std::make_unique<AlphaTestEffect>(device, EffectFlags::None, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating alpha object (except: %s)\n", e.what());
        success = false;
    }

    EffectPipelineStateDescription pdTest(
        &TestVertex::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

    std::unique_ptr<DualTextureEffect> dual;
    try
    {
        // TODO: Fog, VertexColor
        dual = std::make_unique<DualTextureEffect>(device, EffectFlags::None, pdTest);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating dual object (except: %s)\n", e.what());
        success = false;
    }

    std::unique_ptr<EnvironmentMapEffect> envmap;
    try
    {
        // TODO: Fog, Fresnel, PerPixelLightingBit, Specular, BiasedVertexNormals, Specular, Mapping_Sphere, Mapping_DualParabola
        envmap = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::None, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating envmap object (except: %s)\n", e.what());
        success = false;
    }

    std::unique_ptr<SkinnedEffect> skin;
    try
    {
        // TODO: Fog, PerPixelLightingBit, BiasedVertexNormals
        skin = std::make_unique<SkinnedEffect>(device, EffectFlags::None, pdTest);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating skin object (except: %s)\n", e.what());
        success = false;
    }

    return success;
}

_Success_(return)
bool Test11(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    bool success = true;

    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    EffectPipelineStateDescription pd(
        &VertexPositionNormalTexture::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

    std::unique_ptr<NormalMapEffect> nmap;
    try
    {
        // TODO: Fog, Specular, BiasedVertexNormals, VertexColor, Instancing
        nmap = std::make_unique<NormalMapEffect>(device, EffectFlags::None, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        success = false;
    }

    EffectPipelineStateDescription pdTest(
        &TestVertex::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

    std::unique_ptr<SkinnedNormalMapEffect> skin;
    try
    {
        // TODO: Fog, Specular, BiasedVertexNormals
        skin = std::make_unique<SkinnedNormalMapEffect>(device, EffectFlags::None, pdTest);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating skin object (except: %s)\n", e.what());
        success = false;
    }

    std::unique_ptr<DebugEffect> dbg;
    try
    {
        // TODO: Fog, VertexColor, BiasedVertexNormals, Instancing
        dbg = std::make_unique<DebugEffect>(device, EffectFlags::None, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating debug object (except: %s)\n", e.what());
        success = false;
    }

    return success;
}

_Success_(return)
bool Test12(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    bool success = true;

    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    EffectPipelineStateDescription pd(
        &VertexPositionNormalTexture::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

    std::unique_ptr<PBREffect> pbr;
    try
    {
        // TODO: Texture, Emissive, BiasedVertexNormals, Instancing
        pbr = std::make_unique<PBREffect>(device, EffectFlags::None, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        success = false;
    }

    EffectPipelineStateDescription pdTest(
        &TestVertex::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullNone,
        rtState);

    std::unique_ptr<SkinnedPBREffect> skin;
    try
    {
        // TODO: Texture, Emissive, BiasedVertexNormals
        skin = std::make_unique<SkinnedPBREffect>(device, EffectFlags::None, pdTest);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating skin object (except: %s)\n", e.what());
        success = false;
    }

    return success;
}
