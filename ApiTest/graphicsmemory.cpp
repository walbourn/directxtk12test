//--------------------------------------------------------------------------------------
// File: graphicsmemory.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#include "GraphicsMemory.h"

#include <cstdio>
#include <type_traits>

using namespace DirectX;

static_assert(std::is_nothrow_move_constructible<GraphicsResource>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GraphicsResource>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SharedGraphicsResource>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SharedGraphicsResource>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<GraphicsMemory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GraphicsMemory>::value, "Move Assign.");

bool Test05(ID3D12Device*)
{
    //
    // GraphicsMemory is required for the test to run at all. See ApiTest.cpp
    //

    return true;
}
