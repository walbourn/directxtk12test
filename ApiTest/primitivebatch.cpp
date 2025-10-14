//--------------------------------------------------------------------------------------
// File: primitivebatch.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "PrimitiveBatch.h"

#include "VertexTypes.h"

#include <cstdio>
#include <type_traits>

using namespace DirectX;

using Vertex = DirectX::VertexPositionColor;

static_assert(!std::is_copy_constructible<PrimitiveBatch<Vertex>>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<PrimitiveBatch<Vertex>>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<PrimitiveBatch<Vertex>>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<PrimitiveBatch<Vertex>>::value, "Move Assign.");

_Success_(return)
bool Test07(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    bool success = true;

    std::unique_ptr<DirectX::PrimitiveBatch<Vertex>> batch;
    try
    {
        batch = std::make_unique<PrimitiveBatch<Vertex>>(device);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        success = false;
    }

    // invalid args
    try
    {
        ID3D12Device* nullDevice = nullptr;
        auto invalid = std::make_unique<PrimitiveBatch<Vertex>>(nullDevice, 0, 0);

        printf("ERROR: Failed to throw on null device\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        auto invalid = std::make_unique<PrimitiveBatch<Vertex>>(device, 0, 0);

        printf("ERROR: Failed to throw on zero max verts\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        auto invalid = std::make_unique<PrimitiveBatch<Vertex>>(device, 0, 0);

        printf("ERROR: Failed to throw on zero max verts\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        struct BigVertex
        {
            char buffer[4096];
        };

        auto invalid = std::make_unique<PrimitiveBatch<BigVertex>>(device, 4096 * 3, 4096);

        printf("ERROR: Failed to throw on too big vert\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        auto invalid = std::make_unique<PrimitiveBatch<Vertex>>(device, INT32_MAX, 4096);

        printf("ERROR: Failed to throw on too many indices\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        auto invalid = std::make_unique<PrimitiveBatch<Vertex>>(device, 4096 * 3, INT32_MAX);

        printf("ERROR: Failed to throw on too many verts\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    return success;
}
