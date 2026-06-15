// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "GpuRenderer.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

static_assert(sizeof(GpuRenderer::LeafInput) == 48,
    "GpuRenderer::LeafInput must be 48 bytes to match TreeMapCushion.hlsl LeafInput");

namespace
{
    // Constant buffer layout; must be a multiple of 16 bytes.
    struct GpuParams
    {
        UINT  stride;
        float Ia;
        float Is;
        float llx, lly, llz;
        UINT  numLeaves;
        UINT  pad;
    };
    static_assert(sizeof(GpuParams) == 32 && sizeof(GpuParams) % 16 == 0,
        "GpuParams cbuffer must be 32 bytes");

    // Load the embedded HLSL source (plain-text RCDATA resource).
    std::string LoadShaderSource()
    {
        const HRSRC hrsrc = ::FindResource(nullptr,
            MAKEINTRESOURCE(IDR_TREEMAPCUSHION_SHADER), RT_RCDATA);
        if (hrsrc == nullptr) return {};

        const HGLOBAL hglobal = ::LoadResource(nullptr, hrsrc);
        if (hglobal == nullptr) return {};

        const DWORD size = ::SizeofResource(nullptr, hrsrc);
        const auto* data = static_cast<const char*>(::LockResource(hglobal));
        if (data == nullptr || size == 0) return {};

        return { data, size };
    }

    // Singleton that owns all D3D11 state.
    // The immediate context is not thread-safe; m_mutex serialises every call.
    class GpuRendererEngine final
    {
    public:
        static GpuRendererEngine& Get()
        {
            static GpuRendererEngine engine;
            return engine;
        }

        bool IsAvailable()
        {
            std::call_once(m_initFlag, [this] { Initialize(); });
            return m_available;
        }

        bool Render(std::vector<COLORREF>&       bitmapBits,
                    const std::vector<GpuRenderer::LeafInput>& leaves,
                    int stride, float Ia, float lx, float ly, float lz)
        {
            if (!IsAvailable()) return false;
            std::lock_guard lock(m_mutex);

            const auto leafCount   = static_cast<UINT>(leaves.size());
            const auto bitmapCount = static_cast<UINT>(bitmapBits.size());

            if (!EnsureBuffers(leafCount, bitmapCount)) return false;

            // Upload leaf data (only the used portion)
            {
                const D3D11_BOX box{ 0, 0, 0,
                    leafCount * static_cast<UINT>(sizeof(GpuRenderer::LeafInput)), 1, 1 };
                m_context->UpdateSubresource(m_leafBuffer, 0, &box, leaves.data(), 0, 0);
            }

            // Upload the pre-filled bitmap (grid colour already set by BuildLayout)
            m_context->UpdateSubresource(m_bitmapBuffer, 0, nullptr,
                bitmapBits.data(), 0, 0);

            // Upload dispatch parameters
            const GpuParams params{
                static_cast<UINT>(stride), Ia, 1.0f - Ia,
                lx, ly, lz, leafCount, 0
            };
            m_context->UpdateSubresource(m_paramsBuffer, 0, nullptr, &params, 0, 0);

            // Dispatch: ceil(leafCount / 64) groups of 64 threads
            m_context->CSSetShader(m_shader, nullptr, 0);

            ID3D11ShaderResourceView* srv = m_leafSrv;
            m_context->CSSetShaderResources(0, 1, &srv);

            ID3D11UnorderedAccessView* uav = m_bitmapUav;
            m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

            ID3D11Buffer* cb = m_paramsBuffer;
            m_context->CSSetConstantBuffers(0, 1, &cb);

            m_context->Dispatch((leafCount + 63) / 64, 1, 1);

            // Readback: copy GPU output → staging → CPU vector
            m_context->CopyResource(m_bitmapStaging, m_bitmapBuffer);

            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(m_context->Map(m_bitmapStaging, 0, D3D11_MAP_READ, 0, &mapped)))
                return false;

            std::memcpy(bitmapBits.data(), mapped.pData,
                bitmapCount * sizeof(COLORREF));
            m_context->Unmap(m_bitmapStaging, 0);

            return true;
        }

    private:
        GpuRendererEngine() = default;

        void Initialize()
        {
            // Hardware device only; WARP would be slower than the CPU par_unseq path
            constexpr D3D_FEATURE_LEVEL kLevel = D3D_FEATURE_LEVEL_11_0;
            if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                0, &kLevel, 1, D3D11_SDK_VERSION, &m_device, nullptr, &m_context)))
            {
                return;
            }

            const std::string source = LoadShaderSource();
            if (source.empty()) return;

            CComPtr<ID3DBlob> blob, errors;
            if (FAILED(D3DCompile(source.data(), source.size(), "TreeMapCushion.hlsl",
                nullptr, nullptr, "CSCushion", "cs_5_0",
                D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors)))
            {
                return;
            }

            if (FAILED(m_device->CreateComputeShader(
                blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_shader)))
            {
                return;
            }

            // Constant buffer (fixed size, never reallocated)
            D3D11_BUFFER_DESC cbDesc{};
            cbDesc.ByteWidth      = sizeof(GpuParams);
            cbDesc.Usage          = D3D11_USAGE_DEFAULT;
            cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_paramsBuffer))) return;

            m_available = true;
        }

        // Recreate leaf and/or bitmap buffers when sizes exceed current capacity.
        bool EnsureBuffers(UINT leafCount, UINT bitmapCount)
        {
            if (leafCount > m_leafCapacity)
            {
                m_leafSrv.Release();
                m_leafBuffer.Release();

                // Grow by at least 50 % to amortise reallocations
                m_leafCapacity = std::max(leafCount, m_leafCapacity + m_leafCapacity / 2);

                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth           = m_leafCapacity * sizeof(GpuRenderer::LeafInput);
                desc.Usage               = D3D11_USAGE_DEFAULT;
                desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
                desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                desc.StructureByteStride = sizeof(GpuRenderer::LeafInput);
                if (FAILED(m_device->CreateBuffer(&desc, nullptr, &m_leafBuffer)))
                {
                    m_leafCapacity = 0;
                    return false;
                }

                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Format                    = DXGI_FORMAT_UNKNOWN;
                srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_BUFFER;
                srvDesc.Buffer.NumElements        = m_leafCapacity;
                if (FAILED(m_device->CreateShaderResourceView(
                    m_leafBuffer, &srvDesc, &m_leafSrv)))
                {
                    m_leafCapacity = 0;
                    return false;
                }
            }

            if (bitmapCount != m_bitmapCapacity)
            {
                m_bitmapStaging.Release();
                m_bitmapUav.Release();
                m_bitmapBuffer.Release();
                m_bitmapCapacity = bitmapCount;

                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth           = bitmapCount * sizeof(UINT);
                desc.Usage               = D3D11_USAGE_DEFAULT;
                desc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS;
                desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                desc.StructureByteStride = sizeof(UINT);
                if (FAILED(m_device->CreateBuffer(&desc, nullptr, &m_bitmapBuffer)))
                {
                    m_bitmapCapacity = 0;
                    return false;
                }

                D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
                uavDesc.Format              = DXGI_FORMAT_UNKNOWN;
                uavDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
                uavDesc.Buffer.NumElements  = bitmapCount;
                if (FAILED(m_device->CreateUnorderedAccessView(
                    m_bitmapBuffer, &uavDesc, &m_bitmapUav)))
                {
                    m_bitmapCapacity = 0;
                    return false;
                }

                D3D11_BUFFER_DESC stagingDesc{};
                stagingDesc.ByteWidth      = bitmapCount * sizeof(UINT);
                stagingDesc.Usage          = D3D11_USAGE_STAGING;
                stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                if (FAILED(m_device->CreateBuffer(&stagingDesc, nullptr, &m_bitmapStaging)))
                {
                    m_bitmapCapacity = 0;
                    return false;
                }
            }

            return true;
        }

        std::once_flag m_initFlag;
        bool           m_available    = false;
        std::mutex     m_mutex;

        CComPtr<ID3D11Device>           m_device;
        CComPtr<ID3D11DeviceContext>    m_context;
        CComPtr<ID3D11ComputeShader>    m_shader;
        CComPtr<ID3D11Buffer>           m_paramsBuffer;

        // Leaf input buffer (StructuredBuffer<LeafInput> in HLSL)
        CComPtr<ID3D11Buffer>           m_leafBuffer;
        CComPtr<ID3D11ShaderResourceView> m_leafSrv;
        UINT                            m_leafCapacity = 0;

        // Bitmap output buffer (RWStructuredBuffer<uint> in HLSL)
        CComPtr<ID3D11Buffer>           m_bitmapBuffer;
        CComPtr<ID3D11UnorderedAccessView> m_bitmapUav;
        CComPtr<ID3D11Buffer>           m_bitmapStaging;
        UINT                            m_bitmapCapacity = 0;
    };

} // anonymous namespace

bool GpuRenderer::IsAvailable()
{
    return GpuRendererEngine::Get().IsAvailable();
}

bool GpuRenderer::Render(std::vector<COLORREF>&       bitmapBits,
                         const std::vector<LeafInput>& leaves,
                         int   stride,
                         float ambientLight,
                         float lx, float ly, float lz)
{
    return GpuRendererEngine::Get().Render(
        bitmapBits, leaves, stride, ambientLight, lx, ly, lz);
}
