//--------------------------------------------------------------------------------------
// File: TextConsole.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TextConsole.h"

#include "SimpleMath.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cwchar>
#include <utility>

using Microsoft::WRL::ComPtr;

#pragma warning(disable : 26451)

using namespace DirectX;
using namespace DX;

TextConsole::TextConsole() noexcept
    : m_layout{},
    m_textColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0)
{
    Clear();
}


_Use_decl_annotations_
TextConsole::TextConsole(
    ID3D12Device* device,
    ResourceUploadBatch& upload,
    const RenderTargetState& rtState,
    const wchar_t* fontName,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor) noexcept(false)
    : m_layout{},
    m_textColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0)
{
    RestoreDevice(device, upload, rtState, fontName, cpuDescriptor, gpuDescriptor);

    Clear();
}


void TextConsole::Render(_In_ ID3D12GraphicsCommandList* commandList)
{
    if (!m_lines)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    float lineSpacing = m_font->GetLineSpacing();

    float x = float(m_layout.left);
    float y = float(m_layout.top);

    XMVECTOR color = XMLoadFloat4(&m_textColor);

    m_batch->Begin(commandList);

    auto textLine = static_cast<unsigned int>(m_currentLine + 1) % m_rows;

    for (unsigned int line = 0; line < m_rows; ++line)
    {
        XMFLOAT2 pos(x, y + lineSpacing * float(line));

        if (*m_lines[textLine])
        {
            m_font->DrawString(m_batch.get(), m_lines[textLine], pos, color);
        }

        textLine = static_cast<unsigned int>(textLine + 1) % m_rows;
    }

    m_batch->End();
}


void TextConsole::Clear() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buffer)
    {
        memset(m_buffer.get(), 0, sizeof(wchar_t) * (m_columns + 1) * m_rows);
    }

    m_currentColumn = m_currentLine = 0;
}


_Use_decl_annotations_
void TextConsole::Write(const wchar_t* str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessString(str);

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(str);
    }
#endif
}


_Use_decl_annotations_
void TextConsole::WriteLine(const wchar_t* str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessString(str);
    IncrementLine();

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(str);
        OutputDebugStringW(L"\n");
    }
#endif
}


_Use_decl_annotations_
void TextConsole::Format(const wchar_t* strFormat, ...)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    va_list argList;
    va_start(argList, strFormat);

    auto len = size_t(_vscwprintf(strFormat, argList) + 1);

    if (m_tempBuffer.size() < len)
        m_tempBuffer.resize(len);

    memset(m_tempBuffer.data(), 0, sizeof(wchar_t) * len);

    vswprintf_s(m_tempBuffer.data(), m_tempBuffer.size(), strFormat, argList);

    va_end(argList);

    ProcessString(m_tempBuffer.data());

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(m_tempBuffer.data());
    }
#endif
}


void TextConsole::SetWindow(const RECT& layout)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_layout = layout;

    assert(m_font != nullptr);

    float lineSpacing = m_font->GetLineSpacing();
    unsigned int rows = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.bottom - layout.top) / lineSpacing));

    RECT fontLayout = m_font->MeasureDrawBounds(L"X", XMFLOAT2(0, 0));
    unsigned int columns = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.right - layout.left) / float(fontLayout.right - fontLayout.left)));

    auto buffer = std::make_unique<wchar_t[]>((columns + 1) * rows);
    memset(buffer.get(), 0, sizeof(wchar_t) * (columns + 1) * rows);

    auto lines = std::make_unique<wchar_t* []>(rows);
    for (unsigned int line = 0; line < rows; ++line)
    {
        lines[line] = buffer.get() + (columns + 1) * line;
    }

    if (m_lines)
    {
        unsigned int c = std::min<unsigned int>(columns, m_columns);
        unsigned int r = std::min<unsigned int>(rows, m_rows);

        for (unsigned int line = 0; line < r; ++line)
        {
            memcpy(lines[line], m_lines[line], c * sizeof(wchar_t));
        }
    }

    std::swap(columns, m_columns);
    std::swap(rows, m_rows);
    std::swap(buffer, m_buffer);
    std::swap(lines, m_lines);

    if ((m_currentColumn >= m_columns) || (m_currentLine >= m_rows))
    {
        IncrementLine();
    }
}


void TextConsole::ReleaseDevice() noexcept
{
    m_batch.reset();
    m_font.reset();
}

_Use_decl_annotations_
void TextConsole::RestoreDevice(
    ID3D12Device* device,
    ResourceUploadBatch& upload,
    const RenderTargetState& rtState,
    const wchar_t* fontName,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor)

{
    {
        SpriteBatchPipelineStateDescription pd(rtState);
        m_batch = std::make_unique<SpriteBatch>(device, upload, pd);
    }

    m_font = std::make_unique<SpriteFont>(device, upload, fontName, cpuDescriptor, gpuDescriptor);

    m_font->SetDefaultCharacter(L' ');
}


void TextConsole::SetViewport(const D3D12_VIEWPORT& viewPort)
{
    if (m_batch)
    {
        m_batch->SetViewport(viewPort);
    }
}


void TextConsole::SetRotation(DXGI_MODE_ROTATION rotation)
{
    if (m_batch)
    {
        m_batch->SetRotation(rotation);
    }
}


void TextConsole::ProcessString(_In_z_ const wchar_t* str)
{
    if (!m_lines)
        return;

    float width = float(m_layout.right - m_layout.left);

    for (const wchar_t* ch = str; *ch != 0; ++ch)
    {
        if (*ch == '\n')
        {
            IncrementLine();
            continue;
        }

        bool increment = false;

        if (m_currentColumn >= m_columns)
        {
            increment = true;
        }
        else
        {
            m_lines[m_currentLine][m_currentColumn] = *ch;

            auto fontSize = m_font->MeasureString(m_lines[m_currentLine]);
            if (XMVectorGetX(fontSize) > width)
            {
                m_lines[m_currentLine][m_currentColumn] = L'\0';

                increment = true;
            }
        }

        if (increment)
        {
            IncrementLine();
            m_lines[m_currentLine][0] = *ch;
        }

        ++m_currentColumn;
    }
}


void TextConsole::IncrementLine()
{
    if (!m_lines)
        return;

    m_currentLine = (m_currentLine + 1) % m_rows;
    m_currentColumn = 0;
    memset(m_lines[m_currentLine], 0, sizeof(wchar_t) * (m_columns + 1));
}
