//--------------------------------------------------------------------------------------
// File: vertextypes.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#include "VertexTypes.h"

#include "CommonStates.h"
#include "EFfects.h"
#include "EffectPipelineStateDescription.h"
#include "RenderTargetState.h"

#include <cstdio>
#include <type_traits>

using namespace DirectX;

//--------------------------------------------------------------------------------------
// As of DirectXMath 3.13, these types are is_nothrow_copy/move_assignable

// VertexPosition
static_assert(std::is_nothrow_default_constructible<VertexPosition>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPosition>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPosition>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPosition>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPosition>::value, "Move Assign.");

// VertexPositionColor
static_assert(std::is_nothrow_default_constructible<VertexPositionColor>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionColor>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionColor>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionColor>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionColor>::value, "Move Assign.");

// VertexPositionTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionTexture>::value, "Move Assign.");

// VertexPositionDualTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionDualTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionDualTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionDualTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionDualTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionDualTexture>::value, "Move Assign.");

// VertexPositionNormal
static_assert(std::is_nothrow_default_constructible<VertexPositionNormal>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormal>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormal>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormal>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormal>::value, "Move Assign.");

// VertexPositionColorTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionColorTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionColorTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionColorTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionColorTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionColorTexture>::value, "Move Assign.");

// VertexPositionNormalColor
static_assert(std::is_nothrow_default_constructible<VertexPositionNormalColor>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormalColor>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormalColor>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormalColor>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormalColor>::value, "Move Assign.");

// VertexPositionNormalTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionNormalTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormalTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormalTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormalTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormalTexture>::value, "Move Assign.");

// VertexPositionNormalColorTexture
static_assert(std::is_nothrow_default_constructible<VertexPositionNormalColorTexture>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<VertexPositionNormalColorTexture>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<VertexPositionNormalColorTexture>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<VertexPositionNormalColorTexture>::value, "Move Ctor.");
static_assert(std::is_move_assignable<VertexPositionNormalColorTexture>::value, "Move Assign.");

namespace
{
    template<class T>
    _Success_(return)
    inline bool TestVertexType(const RenderTargetState& rtState, _In_ ID3D12Device* device)
    {
        if (T::InputLayout.NumElements == 0
            || T::InputLayout.pInputElementDescs == nullptr)
            return false;

        if (T::InputLayout.NumElements > D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)
            return false;

        if (_stricmp(T::InputLayout.pInputElementDescs[0].SemanticName, "SV_Position") != 0)
        {
            return false;
        }

        for (size_t j = 0; j < T::InputLayout.NumElements; ++j)
        {
            if (T::InputLayout.pInputElementDescs[0].SemanticName == nullptr)
                return false;
        }

        {
            EffectPipelineStateDescription pd(
                &T::InputLayout,
                CommonStates::Opaque,
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
        }

        return true;
    }
}

_Success_(return)
bool Test10(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    bool success = true;

    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    if (!TestVertexType<VertexPosition>(rtState, device))
    {
        printf("ERROR: Failed VertexPosition tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionColor>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionColor tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionTexture>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionTexture tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionDualTexture>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionDualTexture tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionNormal>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionNormal tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionColorTexture>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionColorTexture tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionNormalColor>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionNormalColor tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionNormalTexture>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionNormalTexture tests\n");
        success = false;
    }

    if (!TestVertexType<VertexPositionNormalColorTexture>(rtState, device))
    {
        printf("ERROR: Failed VertexPositionNormalColorTexture tests\n");
        success = false;
    }

    return success;
}
