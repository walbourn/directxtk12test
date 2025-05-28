//--------------------------------------------------------------------------------------
// File: postprocess.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "PostProcess.h"
#include "RenderTargetState.h"

#include <cstdio>
#include <type_traits>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_move_constructible<BasicPostProcess>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<BasicPostProcess>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<DualPostProcess>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DualPostProcess>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ToneMapPostProcess>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ToneMapPostProcess>::value, "Move Assign.");

_Success_(return)
bool Test06(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    bool success = true;

    const RenderTargetState rtState(DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_D32_FLOAT);

    for(size_t j = 0; j < BasicPostProcess::Effect_Max; ++j)
    {
        std::unique_ptr<BasicPostProcess> basic;
        try
        {
            basic = std::make_unique<BasicPostProcess>(device,
                rtState,
                static_cast<BasicPostProcess::Effect>(j));
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating object basic %zu (except: %s)\n", j, e.what());
            success = false;
        }
    }

    for(size_t j = 0; j < 2; ++j)
    {
        std::unique_ptr<DualPostProcess> dual;
        try
        {
            dual = std::make_unique<DualPostProcess>(device,
                rtState,
                j ? DualPostProcess::BloomCombine : DualPostProcess::Merge);
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating object dual %zu (except: %s)\n", j, e.what());
            success = false;
        }
    }

    const RenderTargetState hdrState(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT);

    for(size_t j = 0; j < ToneMapPostProcess::Operator_Max; ++j)
    {
        std::unique_ptr<ToneMapPostProcess> tonemap;
        try
        {
            tonemap = std::make_unique<ToneMapPostProcess>(device,
                hdrState,
                static_cast<ToneMapPostProcess::Operator>(j),
                ToneMapPostProcess::SRGB);
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating object tonemap op %zu (except: %s)\n", j, e.what());
            success = false;
        }

        std::unique_ptr<ToneMapPostProcess> hdr10;
        try
        {
            hdr10 = std::make_unique<ToneMapPostProcess>(device,
                hdrState,
                static_cast<ToneMapPostProcess::Operator>(j),
                ToneMapPostProcess::ST2084);
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating object hdr10 op %zu (except: %s)\n", j, e.what());
            success = false;
        }
    }

    return success;
}
