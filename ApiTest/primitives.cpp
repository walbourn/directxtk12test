//--------------------------------------------------------------------------------------
// File: primitives.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "GeometricPrimitive.h"
#include "ResourceUploadBatch.h"

#include <cstdio>
#include <type_traits>

#include <wrl/client.h>

using namespace DirectX;

static_assert(!std::is_copy_constructible<GeometricPrimitive>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<GeometricPrimitive>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<GeometricPrimitive>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GeometricPrimitive>::value, "Move Assign.");

_Success_(return)
bool Test04(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> copyQueue;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(copyQueue.GetAddressOf()));
    if (FAILED(hr))
    {
        printf("ERROR: Failed creating copy command queue (%08X)\n", static_cast<unsigned int>(hr));
        return false;
    }

    bool success = true;

    std::unique_ptr<GeometricPrimitive> cube;
    try
    {
        cube = GeometricPrimitive::CreateCube(1.f);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating cube (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> box;
    try
    {
        box = GeometricPrimitive::CreateBox(XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f));
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating box (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> sphere;
    try
    {
        sphere = GeometricPrimitive::CreateSphere(1.f, 16);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating sphere (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> geosphere;
    try
    {
        geosphere = GeometricPrimitive::CreateGeoSphere(1.f, 3);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating geosphere (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> cylinder;
    try
    {
        cylinder = GeometricPrimitive::CreateCylinder(1.f, 1.f, 32);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating cylinder (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> cone;
    try
    {
        cone = GeometricPrimitive::CreateCone(1.f, 1.f, 32);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating cone (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> torus;
    try
    {
        torus = GeometricPrimitive::CreateTorus(1.f, 0.333f, 32);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating torus (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> teapot;
    try
    {
        teapot = GeometricPrimitive::CreateTeapot(1.f, 8);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating utah teapot (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> tetra;
    try
    {
        tetra = GeometricPrimitive::CreateTetrahedron(0.75f);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating tetrahedron (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> octa;
    try
    {
        octa = GeometricPrimitive::CreateOctahedron(0.75f);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating octahedron (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> dodec;
    try
    {
        dodec = GeometricPrimitive::CreateDodecahedron(0.5f);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating dodecahedron (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> iso;
    try
    {
        iso = GeometricPrimitive::CreateIcosahedron(0.5f);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating icosadedron (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> customBox;
    try
    {
        GeometricPrimitive::VertexCollection customVerts;
        GeometricPrimitive::IndexCollection customIndices;
        GeometricPrimitive::CreateBox(customVerts, customIndices, XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f));

        assert(customVerts.size() == 24);
        assert(customIndices.size() == 36);

        for (auto& it : customVerts)
        {
            it.textureCoordinate.x *= 5.f;
            it.textureCoordinate.y *= 5.f;
        }

        customBox = GeometricPrimitive::CreateCustom(customVerts, customIndices);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating custom box (except: %s)\n", e.what());
        success =  false;
    }

    std::unique_ptr<GeometricPrimitive> customBox2;
    try
    {
        // Ensure VertexType alias is consistent with alternative client usage
        GeometricPrimitive::VertexCollection customVerts;
        GeometricPrimitive::IndexCollection customIndices;
        GeometricPrimitive::CreateBox(customVerts, customIndices, XMFLOAT3(1.f / 2.f, 2.f / 2.f, 3.f / 2.f));

        assert(customVerts.size() == 24);
        assert(customIndices.size() == 36);

        customBox2 = GeometricPrimitive::CreateCustom(customVerts, customIndices);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating custom (except: %s)\n", e.what());
        success =  false;
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    try
    {
        if (cube)
            cube->LoadStaticBuffers(device, resourceUpload);
        if (box)
            box->LoadStaticBuffers(device, resourceUpload);
        if (sphere)
            sphere->LoadStaticBuffers(device, resourceUpload);
        if (geosphere)
            geosphere->LoadStaticBuffers(device, resourceUpload);
        if (cylinder)
            cylinder->LoadStaticBuffers(device, resourceUpload);
        if (cone)
            cone->LoadStaticBuffers(device, resourceUpload);
        if (torus)
            torus->LoadStaticBuffers(device, resourceUpload);
        if (teapot)
            teapot->LoadStaticBuffers(device, resourceUpload);
        if (tetra)
            tetra->LoadStaticBuffers(device, resourceUpload);
        if (octa)
            octa->LoadStaticBuffers(device, resourceUpload);
        if (dodec)
            dodec->LoadStaticBuffers(device, resourceUpload);
        if (iso)
            iso->LoadStaticBuffers(device, resourceUpload);
        if (customBox)
            customBox->LoadStaticBuffers(device, resourceUpload);
        if (customBox2)
            customBox2->LoadStaticBuffers(device, resourceUpload);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed uploading static buffers (except: %s)\n", e.what());
        success =  false;
    }

    try
    {
        auto uploadResourcesFinished = resourceUpload.End(copyQueue.Get());
        uploadResourcesFinished.wait();
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed doing resource upload via copy command queue (except: %s)\n", e.what());
        return false;
    }

    // invalid args
    try
    {
        GeometricPrimitive::VertexCollection customVerts;
        GeometricPrimitive::IndexCollection customIndices;
        GeometricPrimitive::CreateCustom(customVerts, customIndices);

        printf("ERROR: Failed to throw no verts/indices\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        GeometricPrimitive::VertexCollection customVerts;
        customVerts.resize(1);

        GeometricPrimitive::IndexCollection customIndices;
        customIndices.resize(1);

        GeometricPrimitive::CreateCustom(customVerts, customIndices);

        printf("ERROR: Failed to throw need multiple of 3 indices\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        GeometricPrimitive::VertexCollection customVerts;
        customVerts.resize(1);

        GeometricPrimitive::IndexCollection customIndices;
        customIndices.resize(3);
        customIndices.push_back(0);
        customIndices.push_back(1);
        customIndices.push_back(2);

        GeometricPrimitive::CreateCustom(customVerts, customIndices);

        printf("ERROR: Failed to throw out of range index indices\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        GeometricPrimitive::VertexCollection customVerts;
        customVerts.resize(1);

        GeometricPrimitive::IndexCollection customIndices;
        customIndices.resize(INT32_MAX);

        GeometricPrimitive::CreateCustom(customVerts, customIndices);

        printf("ERROR: Failed to throw too many indices\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

#if defined(_M_X64) || defined(_M_ARM64) || defined(__amd64__) || defined(__aarch64__)
    try
    {
        GeometricPrimitive::VertexCollection customVerts;
        customVerts.resize(UINT32_MAX + 1);

        GeometricPrimitive::IndexCollection customIndices;
        customIndices.resize(3);

        GeometricPrimitive::CreateCustom(customVerts, customIndices);

        printf("ERROR: Failed to throw too many verts\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }
#endif

    return success;
}
