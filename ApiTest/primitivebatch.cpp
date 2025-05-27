//--------------------------------------------------------------------------------------
// File: primitivebatch.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#include "PrimitiveBatch.h"

#include "VertexTypes.h"

#include <cstdio>
#include <type_traits>

using namespace DirectX;

static_assert(std::is_nothrow_move_constructible<PrimitiveBatch<VertexPositionColor>>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<PrimitiveBatch<VertexPositionColor>>::value, "Move Assign.");

_Success_(return)
bool Test07(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    using Vertex = DirectX::VertexPositionColor;

    std::unique_ptr<DirectX::PrimitiveBatch<Vertex>> batch;
    try
    {
        batch = std::make_unique<PrimitiveBatch<Vertex>>(device);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        return false;
    }

    return true;
}
