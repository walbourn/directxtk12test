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
#include "ResourceUploadBatch.h"
#include "VertexTypes.h"

#include <cstdio>
#include <type_traits>

#include <wrl/client.h>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_copy_constructible<RenderTargetState>::value, "Copy Ctor.");
static_assert(std::is_nothrow_copy_assignable<RenderTargetState>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<RenderTargetState>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<RenderTargetState>::value, "Move Assign.");

static_assert(std::is_nothrow_copy_constructible<EffectPipelineStateDescription>::value, "Copy Ctor.");
static_assert(std::is_nothrow_copy_assignable<EffectPipelineStateDescription>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<EffectPipelineStateDescription>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EffectPipelineStateDescription>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<BasicEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<BasicEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<BasicEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<BasicEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<AlphaTestEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<AlphaTestEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<AlphaTestEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<AlphaTestEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<DualTextureEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<DualTextureEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<DualTextureEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DualTextureEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<EnvironmentMapEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<EnvironmentMapEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<EnvironmentMapEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EnvironmentMapEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<SkinnedEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<SkinnedEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<SkinnedEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SkinnedEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<NormalMapEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<NormalMapEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<NormalMapEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<NormalMapEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<SkinnedNormalMapEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<SkinnedNormalMapEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<SkinnedNormalMapEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SkinnedNormalMapEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<PBREffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<PBREffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<PBREffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<PBREffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<SkinnedPBREffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<SkinnedPBREffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<SkinnedPBREffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SkinnedPBREffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<DebugEffect>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<DebugEffect>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<DebugEffect>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DebugEffect>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<EffectTextureFactory>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<EffectTextureFactory>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<EffectTextureFactory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EffectTextureFactory>::value, "Move Assign.");

static_assert(std::is_nothrow_default_constructible<IEffectFactory::EffectInfo>::value, "Copy Ctor.");
static_assert(std::is_copy_constructible<IEffectFactory::EffectInfo>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<IEffectFactory::EffectInfo>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<IEffectFactory::EffectInfo>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<IEffectFactory::EffectInfo>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<EffectFactory>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<EffectFactory>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<EffectFactory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<EffectFactory>::value, "Move Assign.");

static_assert(!std::is_copy_constructible<PBREffectFactory>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<PBREffectFactory>::value, "Copy Assign.");
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

    // invalid args
    #pragma warning(push)
    #pragma warning(disable:6385 6387)
    {
        ID3D12Device* nullDevice = nullptr;
        try
        {
            auto invalid = std::make_unique<BasicEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<BasicEffect>(device, EffectFlags::Instancing, pd);

            printf("ERROR: Failed to throw for unsupported effects flags instancing\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<AlphaTestEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device alpha\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<AlphaTestEffect>(device, EffectFlags::PerPixelLighting, pd);

            printf("ERROR: Failed to throw for unsupported effects flags ppl alpha\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<AlphaTestEffect>(device, EffectFlags::Lighting, pd);

            printf("ERROR: Failed to throw for unsupported effects flags lighting alpha\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<AlphaTestEffect>(device, EffectFlags::Instancing, pd);

            printf("ERROR: Failed to throw for unsupported effects flags instancing alpha\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<DualTextureEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device dual\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<DualTextureEffect>(device, EffectFlags::PerPixelLighting, pd);

            printf("ERROR: Failed to throw for unsupported effects flags ppl dual\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<DualTextureEffect>(device, EffectFlags::Lighting, pd);

            printf("ERROR: Failed to throw for unsupported effects flags lighting dual\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<DualTextureEffect>(device, EffectFlags::Instancing, pd);

            printf("ERROR: Failed to throw for unsupported effects flags instancing dual\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<EnvironmentMapEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device envmap\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::VertexColor, pd);

            printf("ERROR: Failed to throw for unsupported effects flags vertex color envmap\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<EnvironmentMapEffect>(device, EffectFlags::Instancing, pd);

            printf("ERROR: Failed to throw for unsupported effects flags instancing envmap\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedEffect>(device, EffectFlags::VertexColor, pd);

            printf("ERROR: Failed to throw for unsupported effects flags vertex color skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedEffect>(device, EffectFlags::Instancing, pd);

            printf("ERROR: Failed to throw for unsupported effects flags instancing skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<EffectFactory>(nullDevice);

            printf("ERROR: Failed to throw for null device for EffectFactory\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            ID3D12DescriptorHeap* nullHeap = nullptr;
            auto invalid = std::make_unique<EffectFactory>(nullHeap, nullHeap);

            printf("ERROR: Failed to throw for null device for EffectFactory ctor 2\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        ResourceUploadBatch uploadBatch(device);

        try
        {
            ID3D12DescriptorHeap* nullHeap = nullptr;
            auto invalid = std::make_unique<EffectTextureFactory>(nullDevice, uploadBatch, nullHeap);

            printf("ERROR: Failed to throw for null device for EffectTextureFactory\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            ID3D12DescriptorHeap* nullHeap = nullptr;
            auto invalid = std::make_unique<EffectTextureFactory>(device, uploadBatch, nullHeap);

            printf("ERROR: Failed to throw for null heap for EffectTextureFactory\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }
    }
    #pragma warning(pop)

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

    // invalid args
    #pragma warning(push)
    #pragma warning(disable:6385 6387)
    {
        ID3D12Device* nullDevice = nullptr;
        try
        {
            auto invalid = std::make_unique<NormalMapEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedNormalMapEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedNormalMapEffect>(device, EffectFlags::VertexColor, pd);

            printf("ERROR: Failed to throw for unsupported effects flags vertex color skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedNormalMapEffect>(device, EffectFlags::Instancing, pd);

            printf("ERROR: Failed to throw for unsupported effects flags instancing skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<DebugEffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device debug\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<DebugEffect>(device, EffectFlags::None, pd, static_cast<DebugEffect::Mode>(100));

            printf("ERROR: Failed to throw for invalid debug mode\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }
    }
    #pragma warning(pop)

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

    // invalid args
    #pragma warning(push)
    #pragma warning(disable:6385 6387)
    {
        ID3D12Device* nullDevice = nullptr;
        try
        {
            auto invalid = std::make_unique<PBREffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedPBREffect>(nullDevice, EffectFlags::None, pd);

            printf("ERROR: Failed to throw for null device skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedPBREffect>(device, EffectFlags::Instancing, pd);

            printf("ERROR: Failed to throw for unsupported effects flags instancing skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<SkinnedPBREffect>(device, EffectFlags::Velocity, pd);

            printf("ERROR: Failed to throw for unsupported effects flags velocity skin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            auto invalid = std::make_unique<PBREffectFactory>(nullDevice);

            printf("ERROR: Failed to throw for null device for PBREffectFactory\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            ID3D12DescriptorHeap* nullHeap = nullptr;
            auto invalid = std::make_unique<PBREffectFactory>(nullHeap, nullHeap);

            printf("ERROR: Failed to throw for null device for PBREffectFactory ctor 2\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }
    }
    #pragma warning(pop)

    return success;
}
