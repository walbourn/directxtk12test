//--------------------------------------------------------------------------------------
// File: commonstates.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#include "CommonStates.h"

#include "Effects.h"
#include "EffectPipelineStateDescription.h"
#include "VertexTypes.h"

#include <cstdio>
#include <type_traits>

using namespace DirectX;

static_assert(std::is_nothrow_move_constructible<CommonStates>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<CommonStates>::value, "Move Assign.");

namespace
{
    bool TestBlendState(const D3D12_BLEND_DESC& state, const RenderTargetState& rtState, _In_ ID3D12Device* device)
    {
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            state,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        try
        {
            auto effect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
            if (!effect)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    bool TestDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& state, const RenderTargetState& rtState, _In_ ID3D12Device* device)
    {
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            CommonStates::Opaque,
            state,
            CommonStates::CullNone,
            rtState);

        try
        {
            auto effect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
            if (!effect)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return true;
    }

    bool TestRasterizerState(const D3D12_RASTERIZER_DESC& state, const RenderTargetState& rtState, _In_ ID3D12Device* device)
    {
        EffectPipelineStateDescription pd(
            &VertexPositionColor::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            state,
            rtState);

        try
        {
            auto effect = std::make_unique<BasicEffect>(device, EffectFlags::None, pd);
            if (!effect)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        return true;
    }
}

bool Test02(ID3D12Device* device)
{
    std::unique_ptr<CommonStates> states;
    try
    {
        states = std::make_unique<CommonStates>(device);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        return false;
    }

    bool success = true;

    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    if (!TestBlendState(CommonStates::Opaque, rtState, device)
        || !TestBlendState(CommonStates::AlphaBlend, rtState, device)
        || !TestBlendState(CommonStates::Additive, rtState, device)
        || !TestBlendState(CommonStates::NonPremultiplied, rtState, device))
    {
        printf("ERROR: Failed CommonStates blend state tests\n");
        success = false;
    }

    if (!TestDepthStencilState(CommonStates::DepthNone, rtState, device)
        || !TestDepthStencilState(CommonStates::DepthDefault, rtState, device)
        || !TestDepthStencilState(CommonStates::DepthRead, rtState, device)
        || !TestDepthStencilState(CommonStates::DepthReverseZ, rtState, device)
        || !TestDepthStencilState(CommonStates::DepthReadReverseZ, rtState, device))
    {
        printf("ERROR: Failed CommonStates depth/stencil state tests\n");
        success = false;
    }

    if (!TestRasterizerState(CommonStates::CullNone, rtState, device)
        || !TestRasterizerState(CommonStates::CullClockwise, rtState, device)
        || !TestRasterizerState(CommonStates::CullCounterClockwise, rtState, device)
        || !TestRasterizerState(CommonStates::Wireframe, rtState, device))
    {
        printf("ERROR: Failed CommonStates rasterizer state tests\n");
        success = false;
    }

    {
        auto desc1 = CommonStates::StaticPointWrap(2);
        auto desc2 = CommonStates::StaticPointClamp(3);
        auto desc3 = CommonStates::StaticLinearWrap(1);
        auto desc4 = CommonStates::StaticLinearClamp(8);
        auto desc5 = CommonStates::StaticAnisotropicWrap(4);
        auto desc6 = CommonStates::StaticAnisotropicClamp(6);
        if (desc1.ShaderRegister != 2
            || desc2.ShaderRegister != 3
            || desc3.ShaderRegister != 1
            || desc4.ShaderRegister != 8
            || desc5.ShaderRegister != 4
            || desc6.ShaderRegister != 6)
        {
            printf("ERROR: Failed CommonStates static sampler state tests\n");
            success = false;
        }
    }

    if (states->Heap() == nullptr
        || states->PointWrap().ptr == 0
        || states->PointClamp().ptr == 0
        || states->LinearWrap().ptr == 0
        || states->LinearClamp().ptr == 0
        || states->AnisotropicWrap().ptr == 0
        || states->AnisotropicClamp().ptr == 0)
    {
        printf("ERROR: Failed CommonStates heap sampler state tests\n");
        success = false;
    }

    return success;
}
