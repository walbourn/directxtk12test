//--------------------------------------------------------------------------------------
// File: fuzzloaders.cpp
//
// Simple command-line tool for fuzz-testing the texture and sound loaders.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4005)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#pragma warning(pop)

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <list>
#include <memory>
#include <vector>

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "WaveBankReader.h"
#include "WAVFileReader.h"

#include <wrl\client.h>

using Microsoft::WRL::ComPtr;

namespace
{
    struct handle_closer { void operator()(HANDLE h) { if (h) CloseHandle(h); } };

    using ScopedHandle = std::unique_ptr<void, handle_closer>;

    inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    struct find_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) FindClose(h); } };

    using ScopedFindHandle = std::unique_ptr<void, find_closer>;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

enum OPTIONS
{
    OPT_RECURSIVE = 1,
    OPT_DDS,
    OPT_WAV,
    OPT_WIC,
    OPT_XWB,
    OPT_MAX
};

static_assert(OPT_MAX <= 32, "dwOptions is a DWORD bitfield");

struct SConversion
{
    wchar_t szSrc[MAX_PATH];
};

struct SValue
{
    LPCWSTR pName;
    DWORD dwValue;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

const SValue g_pOptions [] =
{
    { L"r",         OPT_RECURSIVE },
    { L"dds",       OPT_DDS },
    { L"wav",       OPT_WAV },
    { L"wic",       OPT_WIC },
    { L"xwb",       OPT_XWB },
    { nullptr,      0 }
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

namespace
{
#ifdef _PREFAST_
#pragma prefast(disable : 26018, "Only used with static internal arrays")
#endif

    DWORD LookupByName(const wchar_t *pName, const SValue *pArray)
    {
        while (pArray->pName)
        {
            if (!_wcsicmp(pName, pArray->pName))
                return pArray->dwValue;

            pArray++;
        }

        return 0;
    }

    void SearchForFiles(const wchar_t* path, std::list<SConversion>& files, bool recursive)
    {
        // Process files
        WIN32_FIND_DATA findData = {};
        ScopedFindHandle hFile(safe_handle(FindFirstFileExW(path,
            FindExInfoBasic, &findData,
            FindExSearchNameMatch, nullptr,
            FIND_FIRST_EX_LARGE_FETCH)));
        if (hFile)
        {
            for (;;)
            {
                if (!(findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY)))
                {
                    wchar_t drive[_MAX_DRIVE] = {};
                    wchar_t dir[_MAX_DIR] = {};
                    _wsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);

                    SConversion conv;
                    _wmakepath_s(conv.szSrc, drive, dir, findData.cFileName, nullptr);
                    files.push_back(conv);
                }

                if (!FindNextFile(hFile.get(), &findData))
                    break;
            }
        }
            
        // Process directories
        if (recursive)
        {
            wchar_t searchDir[MAX_PATH] = {};
            {
                wchar_t drive[_MAX_DRIVE] = {};
                wchar_t dir[_MAX_DIR] = {};
                _wsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
                _wmakepath_s(searchDir, drive, dir, L"*", nullptr);
            }

            hFile.reset(safe_handle(FindFirstFileExW(searchDir,
                FindExInfoBasic, &findData,
                FindExSearchLimitToDirectories, nullptr,
                FIND_FIRST_EX_LARGE_FETCH)));
            if (!hFile)
                return;

            for (;;)
            {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (findData.cFileName[0] != L'.')
                    {
                        wchar_t subdir[MAX_PATH] = {};

                        {
                            wchar_t drive[_MAX_DRIVE] = {};
                            wchar_t dir[_MAX_DIR] = {};
                            wchar_t fname[_MAX_FNAME] = {};
                            wchar_t ext[_MAX_FNAME] = {};
                            _wsplitpath_s(path, drive, dir, fname, ext);
                            wcscat_s(dir, findData.cFileName);
                            _wmakepath_s(subdir, drive, dir, fname, ext);
                        }

                        SearchForFiles(subdir, files, recursive);
                    }
                }

                if (!FindNextFile(hFile.get(), &findData))
                    break;
            }
        }
    }

    void PrintUsage()
    {
        wprintf(L"DirectX Tool Kit for DX12\n\n");
        wprintf(L"Usage: fuzzloaders <options> <files>\n");
        wprintf(L"\n");
        wprintf(L"   -r                  wildcard filename search is recursive\n");
        wprintf(L"   -dds                force use of DDSTextureLoader\n");
        wprintf(L"   -wav                force use of WAVFileReader\n");
        wprintf(L"   -wic                force use of WICTextureLoader\n");
        wprintf(L"   -xwb                force use of WaveBankReader\n");
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

HRESULT CreateDevice(ID3D12Device** pDev)
{
    HRESULT hr = E_FAIL;

    static IDXGIFactory4* s_dxgiFactory = nullptr;

    if (!s_dxgiFactory)
    {
        hr = CreateDXGIFactory1(IID_PPV_ARGS(&s_dxgiFactory));
        if (FAILED(hr))
            return hr;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != s_dxgiFactory->EnumAdapters1(adapterIndex, adapter.ReleaseAndGetAddressOf()); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->GetDesc1(&desc)))
        {
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

    if (!adapter)
    {
        // Try WARP12 instead
        if (FAILED(s_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
        {
            return E_FAIL;
        }
    }

    return D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(pDev));
}

//--------------------------------------------------------------------------------------
// Entry-point
//--------------------------------------------------------------------------------------
#ifdef _PREFAST_
#pragma prefast(disable : 28198, "Command-line tool, frees all memory on exit")
#endif

int __cdecl wmain(_In_ int argc, _In_z_count_(argc) wchar_t* argv[])
{
    // Initialize COM (needed for WIC)
    HRESULT hr = hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to initialize COM (%08X)\n", static_cast<unsigned int>(hr));
        return 1;
    }

    // Process command line
    DWORD dwOptions = 0;
    std::list<SConversion> conversion;

    for (int iArg = 1; iArg < argc; iArg++)
    {
        PWSTR pArg = argv[iArg];

        if (('-' == pArg[0]) || ('/' == pArg[0]))
        {
            pArg++;
            PWSTR pValue;

            for (pValue = pArg; *pValue && (':' != *pValue); pValue++);

            if (*pValue)
                *pValue++ = 0;

            DWORD dwOption = LookupByName(pArg, g_pOptions);

            if (!dwOption || (dwOptions & (1 << dwOption)))
            {
                PrintUsage();
                return 1;
            }

            dwOptions |= 1 << dwOption;

            switch (dwOption)
            {
            case OPT_DDS:
            case OPT_WAV:
            case OPT_WIC:
            case OPT_XWB:
                if ((dwOptions & ((1 << OPT_DDS) | (1 << OPT_WAV) | (1 << OPT_WIC) | (1 << OPT_XWB))) & ~(1 << dwOption))
                {
                    wprintf(L"-dds, -wav, -wic, and -xwb are mutually exclusive options\n");
                    return 1;
                }
                break;
            }
        }
        else if (wcspbrk(pArg, L"?*") != nullptr)
        {
            size_t count = conversion.size();
            SearchForFiles(pArg, conversion, (dwOptions & (1 << OPT_RECURSIVE)) != 0);
            if (conversion.size() <= count)
            {
                wprintf(L"No matching files found for %ls\n", pArg);
                return 1;
            }
        }
        else
        {
            SConversion conv;
            wcscpy_s(conv.szSrc, MAX_PATH, pArg);

            conversion.push_back(conv);
        }
    }

    if (conversion.empty())
    {
        wprintf(L"ERROR: Need at least 1 image file to fuzz\n\n");
        PrintUsage();
        return 0;
    }

    ComPtr<ID3D12Device> device;
    hr = CreateDevice(device.GetAddressOf());
    if (FAILED(hr))
    {
        wprintf(L"ERROR: Failed to create required Direct3D device to fuzz: %08X\n", static_cast<unsigned int>(hr));
        return 1;
    }

    for (auto& pConv : conversion)
    {
        wchar_t ext[_MAX_EXT];
        _wsplitpath_s(pConv.szSrc, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);
        bool isdds = (_wcsicmp(ext, L".dds") == 0);
        bool iswav = (_wcsicmp(ext, L".wav") == 0);
        bool isxwb = (_wcsicmp(ext, L".xwb") == 0);

        bool usedds = false;
        bool usewav = false;
        bool usexwb = false;
        if (dwOptions & (1 << OPT_DDS))
        {
            usedds = true;
        }
        else if (dwOptions & (1 << OPT_WAV))
        {
            usewav = true;
        }
        else if (dwOptions & (1 << OPT_XWB))
        {
            usexwb = true;
        }
        else if (!(dwOptions & (1 << OPT_WIC)))
        {
            usedds = isdds;
            usewav = iswav;
            usexwb = isxwb;
        }

        // Load source image
#ifdef _DEBUG
        OutputDebugStringW(pConv.szSrc);
        OutputDebugStringA("\n");
#endif

        ComPtr<ID3D12Resource> tex;
        std::unique_ptr<uint8_t[]> texData;
        if (usedds)
        {
            std::vector<D3D12_SUBRESOURCE_DATA> texRes;
            hr = DirectX::LoadDDSTextureFromFile(device.Get(), pConv.szSrc, tex.GetAddressOf(), texData, texRes, 0, nullptr, nullptr);
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                wprintf(L"ERROR: DDSTexture file not not found:\n%ls\n", pConv.szSrc);
                return 1;
            }
            else if (FAILED(hr) && hr != E_INVALIDARG && hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) && hr != E_OUTOFMEMORY && hr != HRESULT_FROM_WIN32(ERROR_HANDLE_EOF) && (hr != E_FAIL || (hr == E_FAIL && isdds)))
            {
#ifdef _DEBUG
                char buff[128] = {};
                sprintf_s(buff, "DDSTexture failed with %08X\n", static_cast<unsigned int>(hr));
                OutputDebugStringA(buff);
#endif
                wprintf(L"!");
            }
            else
            {
                wprintf(SUCCEEDED(hr) ? L"*" : L".");
            }
        }
        else if (usewav)
        {
            std::unique_ptr<uint8_t[]> data;
            DirectX::WAVData result = {};
            hr = DirectX::LoadWAVAudioFromFileEx(pConv.szSrc, data, result);
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                wprintf(L"ERROR: WAVAudio file not not found:\n%ls\n", pConv.szSrc);
                return 1;
            }
            else if (FAILED(hr) && hr != E_INVALIDARG && hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) && hr != E_OUTOFMEMORY && hr != HRESULT_FROM_WIN32(ERROR_INVALID_DATA) && hr != HRESULT_FROM_WIN32(ERROR_HANDLE_EOF) && (hr != E_FAIL || (hr == E_FAIL && iswav)))
            {
#ifdef _DEBUG
                char buff[128] = {};
                sprintf_s(buff, "WAVAudio failed with %08X\n", static_cast<unsigned int>(hr));
                OutputDebugStringA(buff);
#endif
                wprintf(L"!");
            }
            else
            {
                wprintf(SUCCEEDED(hr) ? L"*" : L".");
            }
        }
        else if (usexwb)
        {
            auto wb = std::make_unique<DirectX::WaveBankReader>();
            hr = wb->Open(pConv.szSrc);
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                wprintf(L"ERROR: XWBAudio file not not found:\n%ls\n", pConv.szSrc);
                return 1;
            }
            else if (FAILED(hr) && hr != E_INVALIDARG && hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) && hr != E_OUTOFMEMORY && hr != HRESULT_FROM_WIN32(ERROR_HANDLE_EOF) && (hr != E_FAIL || (hr == E_FAIL && isxwb)))
            {
#ifdef _DEBUG
                char buff[128] = {};
                sprintf_s(buff, "XWBAudio failed with %08X\n", static_cast<unsigned int>(hr));
                OutputDebugStringA(buff);
#endif
                wprintf(L"!");
            }
            else if (SUCCEEDED(hr))
            {
                wprintf(L"w");
                wb->WaitOnPrepare();
                wprintf(L"\b*");
            }
            else
            {
                wprintf(L".");
            }
        }
        else
        {
            D3D12_SUBRESOURCE_DATA texRes;
            hr = DirectX::LoadWICTextureFromFile(device.Get(), pConv.szSrc, tex.GetAddressOf(), texData, texRes, 0);
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                wprintf(L"ERROR: WICTexture file not found:\n%ls\n", pConv.szSrc);
                return 1;
            }
            else if (FAILED(hr) && hr != E_INVALIDARG && hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) && hr != WINCODEC_ERR_COMPONENTNOTFOUND && hr != E_OUTOFMEMORY && hr != WINCODEC_ERR_BADHEADER)
            {
#ifdef _DEBUG
                char buff[128] = {};
                sprintf_s(buff, "WICTexture failed with %08X\n", static_cast<unsigned int>(hr));
                OutputDebugStringA(buff);
#endif
                wprintf(L"!");
            }
            else
            {
                wprintf(SUCCEEDED(hr) ? L"*" : L".");
            }
        }
        fflush(stdout);
    }

    wprintf(L"\n*** FUZZING COMPLETE ***\n");

    return 0;
}
