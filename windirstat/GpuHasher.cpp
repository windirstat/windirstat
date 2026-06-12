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
#include "GpuHasher.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace
{
    // Bytes of message data uploaded per dispatch. Each dispatch runs a
    // single GPU thread over BlockCount blocks, so this value also bounds
    // the time spent in one dispatch — keep it well below the ~2 second
    // Windows TDR (GPU timeout) limit even on slow hardware.
    constexpr UINT kChunkBytes = 4 * wds::Mi;

    // Headroom for the padded final blocks (at most two extra blocks of
    // the largest block size).
    constexpr UINT kInputCapacity = kChunkBytes + 512;

    // The persistent state buffer is sized for the largest algorithm:
    // SHA512 keeps 8 emulated 64-bit words = 16 uints = 64 bytes.
    constexpr UINT kStateWordsMax = 16;

    // The four kernels in GpuHash.hlsl. SHA384 reuses the SHA512 kernel;
    // it differs only in its IV and digest truncation.
    enum GpuKernel : std::uint8_t
    {
        KERNEL_MD5,
        KERNEL_SHA1,
        KERNEL_SHA256,
        KERNEL_SHA512,
        KERNEL_COUNT
    };

    constexpr std::array<LPCSTR, KERNEL_COUNT> kKernelEntryPoints =
    {
        "CSMD5", "CSSHA1", "CSSHA256", "CSSHA512"
    };

    struct AlgoSpec
    {
        GpuKernel kernel;     // which compute shader processes the blocks
        UINT blockSize;       // message block size in bytes (64 or 128)
        UINT stateWords;      // 32-bit words of persistent state
        UINT digestBytes;     // size of the final digest
        bool is64Bit;         // state words are emulated 64-bit (lo,hi) pairs
        bool littleEndian;    // MD5 is little-endian, the SHA family big-endian
        std::array<UINT, kStateWordsMax> iv; // initialization vector
    };

    // Initialization vectors from RFC 1321 (MD5) and FIPS 180-4 section 5.3.
    // For the 64-bit algorithms each word is stored as (lo, hi) to match
    // the uint2 layout of the shader.
    const std::array<AlgoSpec, 5> kAlgoSpecs =
    { {
        // HASH_MD5
        { KERNEL_MD5, 64, 4, 16, false, true,
          { 0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u } },
        // HASH_SHA1
        { KERNEL_SHA1, 64, 5, 20, false, false,
          { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u } },
        // HASH_SHA256
        { KERNEL_SHA256, 64, 8, 32, false, false,
          { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u } },
        // HASH_SHA384 (SHA512 kernel, truncated to 48 bytes)
        { KERNEL_SHA512, 128, 16, 48, true, false,
          { 0xc1059ed8u, 0xcbbb9d5du, 0x367cd507u, 0x629a292au,
            0x3070dd17u, 0x9159015au, 0xf70e5939u, 0x152fecd8u,
            0xffc00b31u, 0x67332667u, 0x68581511u, 0x8eb44a87u,
            0x64f98fa7u, 0xdb0c2e0du, 0xbefa4fa4u, 0x47b5481du } },
        // HASH_SHA512
        { KERNEL_SHA512, 128, 16, 64, true, false,
          { 0xf3bcc908u, 0x6a09e667u, 0x84caa73bu, 0xbb67ae85u,
            0xfe94f82bu, 0x3c6ef372u, 0x5f1d36f1u, 0xa54ff53au,
            0xade682d1u, 0x510e527fu, 0x2b3e6c1fu, 0x9b05688cu,
            0xfb41bd6bu, 0x1f83d9abu, 0x137e2179u, 0x5be0cd19u } }
    } };

    // Load the embedded HLSL source (plain text RCDATA resource)
    std::string LoadShaderSource()
    {
        const HRSRC hrsrc = ::FindResource(nullptr, MAKEINTRESOURCE(IDR_GPUHASH_SHADER), RT_RCDATA);
        if (hrsrc == nullptr) return {};

        const HGLOBAL hglobal = ::LoadResource(nullptr, hrsrc);
        if (hglobal == nullptr) return {};

        const DWORD size = ::SizeofResource(nullptr, hrsrc);
        const auto data = static_cast<const char*>(::LockResource(hglobal));
        if (data == nullptr || size == 0) return {};

        return { data, size };
    }

    //
    // GpuHashEngine. Owns the D3D11 device, the compiled kernels and the
    // GPU buffers. A single engine instance is shared by all threads; the
    // immediate context is not thread-safe, so every hash operation runs
    // under m_mutex (the GPU is a single shared resource anyway).
    //
    class GpuHashEngine final
    {
    public:
        static GpuHashEngine& Get()
        {
            static GpuHashEngine engine;
            return engine;
        }

        bool IsAvailable()
        {
            std::call_once(m_initFlag, [this] { Initialize(); });
            return m_available;
        }

        // Hash a complete in-memory message
        std::vector<BYTE> HashBuffer(const BYTE* data, const size_t size, const HashAlgorithm algo)
        {
            if (!IsAvailable() || data == nullptr) return {};

            std::lock_guard lock(m_mutex);
            const AlgoSpec& spec = kAlgoSpecs[algo];

            BeginSession(spec);
            ULONGLONG totalBytes = 0;
            std::vector<BYTE> carry;
            UpdateSession(spec, data, size, carry, totalBytes);
            return FinishSession(spec, carry, totalBytes);
        }

        // Hash up to sizeLimit bytes of a file, reading chunk by chunk
        std::vector<BYTE> HashFile(const std::wstring& path, const ULONGLONG sizeLimit,
            const HashAlgorithm algo, const std::function<void()>& progressCallback)
        {
            if (!IsAvailable()) return {};

            // Same sharing/backup semantics as the BCrypt path in
            // CItem::GetFileHash so both paths see identical file content
            const SmartPointer hFile(CloseHandle, CreateFile(path.c_str(),
                GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
            if (hFile == INVALID_HANDLE_VALUE) return {};

            std::lock_guard lock(m_mutex);
            const AlgoSpec& spec = kAlgoSpecs[algo];

            BeginSession(spec);
            thread_local std::vector<BYTE> fileBuffer(kChunkBytes);
            std::vector<BYTE> carry;
            ULONGLONG totalBytes = 0;

            DWORD readBytes = 0;
            BOOL readResult = TRUE;
            while ((readResult = ReadFile(hFile, fileBuffer.data(), static_cast<DWORD>(
                std::min<ULONGLONG>(sizeLimit - totalBytes, fileBuffer.size())),
                &readBytes, nullptr)) != 0 && readBytes > 0)
            {
                if (progressCallback) progressCallback();

                UpdateSession(spec, fileBuffer.data(), readBytes, carry, totalBytes);
                if (totalBytes >= sizeLimit) break;
            }

            // A read error must not yield a digest of partial data
            if (readResult == 0) return {};

            return FinishSession(spec, carry, totalBytes);
        }

    private:
        GpuHashEngine() = default;

        void Initialize()
        {
            // Hardware only: WARP would technically work but is slower
            // than the BCrypt CPU path, so it must never count as a GPU
            constexpr D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_11_0;
            if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                0, &requestedLevel, 1, D3D11_SDK_VERSION, &m_device, nullptr, &m_context)))
            {
                return;
            }

            const std::string source = LoadShaderSource();
            if (source.empty()) return;

            // Compile all kernels up front; a single failure disables GPU
            // hashing entirely so behavior is consistent across algorithms
            for (UINT i = 0; i < KERNEL_COUNT; ++i)
            {
                CComPtr<ID3DBlob> blob;
                CComPtr<ID3DBlob> errors;
                if (FAILED(D3DCompile(source.data(), source.size(), "GpuHash.hlsl",
                    nullptr, nullptr, kKernelEntryPoints[i], "cs_5_0",
                    D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors)) ||
                    FAILED(m_device->CreateComputeShader(blob->GetBufferPointer(),
                    blob->GetBufferSize(), nullptr, &m_shaders[i])))
                {
                    return;
                }
            }

            // Input data buffer (structured uints, updated per dispatch)
            D3D11_BUFFER_DESC inputDesc = {};
            inputDesc.ByteWidth = kInputCapacity;
            inputDesc.Usage = D3D11_USAGE_DEFAULT;
            inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            inputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            inputDesc.StructureByteStride = sizeof(UINT);
            if (FAILED(m_device->CreateBuffer(&inputDesc, nullptr, &m_inputBuffer))) return;

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.NumElements = kInputCapacity / sizeof(UINT);
            if (FAILED(m_device->CreateShaderResourceView(m_inputBuffer, &srvDesc, &m_inputSrv))) return;

            // Persistent hash state buffer + UAV
            D3D11_BUFFER_DESC stateDesc = {};
            stateDesc.ByteWidth = kStateWordsMax * sizeof(UINT);
            stateDesc.Usage = D3D11_USAGE_DEFAULT;
            stateDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            stateDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            stateDesc.StructureByteStride = sizeof(UINT);
            if (FAILED(m_device->CreateBuffer(&stateDesc, nullptr, &m_stateBuffer))) return;

            D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.NumElements = kStateWordsMax;
            if (FAILED(m_device->CreateUnorderedAccessView(m_stateBuffer, &uavDesc, &m_stateUav))) return;

            // Staging copy of the state buffer for CPU readback
            D3D11_BUFFER_DESC stagingDesc = {};
            stagingDesc.ByteWidth = kStateWordsMax * sizeof(UINT);
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (FAILED(m_device->CreateBuffer(&stagingDesc, nullptr, &m_stateStaging))) return;

            // Constant buffer carrying the per-dispatch block count
            D3D11_BUFFER_DESC paramsDesc = {};
            paramsDesc.ByteWidth = 4 * sizeof(UINT);
            paramsDesc.Usage = D3D11_USAGE_DEFAULT;
            paramsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            if (FAILED(m_device->CreateBuffer(&paramsDesc, nullptr, &m_paramsBuffer))) return;

            m_available = true;
        }

        // Reset the persistent state buffer to the algorithm's IV
        void BeginSession(const AlgoSpec& spec) const
        {
            m_context->UpdateSubresource(m_stateBuffer, 0, nullptr, spec.iv.data(), 0, 0);
        }

        // Feed message bytes into the session. Whole blocks are dispatched
        // directly; a partial trailing block is kept in `carry` until more
        // data arrives or the session is finished.
        void UpdateSession(const AlgoSpec& spec, const BYTE* data, size_t size,
            std::vector<BYTE>& carry, ULONGLONG& totalBytes) const
        {
            totalBytes += size;

            // Top up a pending partial block first
            if (!carry.empty())
            {
                const size_t needed = std::min<size_t>(spec.blockSize - carry.size(), size);
                carry.insert(carry.end(), data, data + needed);
                data += needed;
                size -= needed;

                if (carry.size() < spec.blockSize) return;
                DispatchBlocks(spec, carry.data(), spec.blockSize);
                carry.clear();
            }

            // Dispatch all whole blocks directly from the caller's buffer
            const size_t wholeBytes = size - size % spec.blockSize;
            for (size_t offset = 0; offset < wholeBytes; offset += kChunkBytes)
            {
                DispatchBlocks(spec, data + offset,
                    static_cast<UINT>(std::min<size_t>(kChunkBytes, wholeBytes - offset)));
            }

            // Keep the trailing partial block for later
            carry.assign(data + wholeBytes, data + size);
        }

        // Apply the Merkle-Damgård padding, run the final blocks and read
        // back the digest. Padding: 0x80, zeros, then the message length
        // in bits (64-bit field, 128-bit for SHA384/512).
        std::vector<BYTE> FinishSession(const AlgoSpec& spec,
            std::vector<BYTE>& carry, const ULONGLONG totalBytes) const
        {
            const UINT lengthField = spec.is64Bit ? 16 : 8;
            carry.push_back(0x80);
            while (carry.size() % spec.blockSize != spec.blockSize - lengthField)
            {
                carry.push_back(0x00);
            }

            const ULONGLONG bitLength = totalBytes * 8;
            if (spec.littleEndian)
            {
                // MD5: 64-bit length, little-endian
                for (UINT i = 0; i < 8; ++i)
                {
                    carry.push_back(static_cast<BYTE>(bitLength >> (8 * i)));
                }
            }
            else
            {
                // SHA family: big-endian; for the 128-bit field the upper
                // 64 bits are always zero (file sizes fit into 64 bits)
                for (UINT i = 8; i < lengthField; ++i)
                {
                    carry.push_back(0x00);
                }
                for (int i = 7; i >= 0; --i)
                {
                    carry.push_back(static_cast<BYTE>(bitLength >> (8 * i)));
                }
            }

            DispatchBlocks(spec, carry.data(), static_cast<UINT>(carry.size()));
            carry.clear();

            return ReadDigest(spec);
        }

        // Upload one batch of whole blocks and run the kernel over them
        void DispatchBlocks(const AlgoSpec& spec, const BYTE* data, const UINT byteCount) const
        {
            ASSERT(byteCount % spec.blockSize == 0 && byteCount <= kInputCapacity);

            const D3D11_BOX box = { 0, 0, 0, byteCount, 1, 1 };
            m_context->UpdateSubresource(m_inputBuffer, 0, &box, data, 0, 0);

            const UINT params[4] = { byteCount / spec.blockSize, 0, 0, 0 };
            m_context->UpdateSubresource(m_paramsBuffer, 0, nullptr, params, 0, 0);

            m_context->CSSetShader(m_shaders[spec.kernel], nullptr, 0);
            ID3D11ShaderResourceView* srv = m_inputSrv;
            m_context->CSSetShaderResources(0, 1, &srv);
            ID3D11UnorderedAccessView* uav = m_stateUav;
            m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
            ID3D11Buffer* cb = m_paramsBuffer;
            m_context->CSSetConstantBuffers(0, 1, &cb);
            m_context->Dispatch(1, 1, 1);
        }

        // Copy the state to the staging buffer and convert it to the
        // canonical digest byte order
        std::vector<BYTE> ReadDigest(const AlgoSpec& spec) const
        {
            m_context->CopyResource(m_stateStaging, m_stateBuffer);

            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(m_context->Map(m_stateStaging, 0, D3D11_MAP_READ, 0, &mapped)))
            {
                return {};
            }

            std::array<UINT, kStateWordsMax> state{};
            std::memcpy(state.data(), mapped.pData, sizeof(state));
            m_context->Unmap(m_stateStaging, 0);

            std::vector<BYTE> digest;
            digest.reserve(spec.digestBytes);
            if (spec.is64Bit)
            {
                // State is (lo, hi) pairs; the digest is the big-endian
                // bytes of each 64-bit word: hi word first, then lo word
                for (UINT i = 0; digest.size() < spec.digestBytes; ++i)
                {
                    const UINT hi = state[2 * i + 1];
                    const UINT lo = state[2 * i];
                    for (int b = 3; b >= 0; --b) digest.push_back(static_cast<BYTE>(hi >> (8 * b)));
                    for (int b = 3; b >= 0; --b) digest.push_back(static_cast<BYTE>(lo >> (8 * b)));
                }
            }
            else if (spec.littleEndian)
            {
                // MD5: state words are emitted as little-endian bytes
                for (UINT i = 0; i < spec.stateWords; ++i)
                {
                    for (UINT b = 0; b < 4; ++b) digest.push_back(static_cast<BYTE>(state[i] >> (8 * b)));
                }
            }
            else
            {
                // SHA1/SHA256: state words are emitted as big-endian bytes
                for (UINT i = 0; i < spec.stateWords; ++i)
                {
                    for (int b = 3; b >= 0; --b) digest.push_back(static_cast<BYTE>(state[i] >> (8 * b)));
                }
            }

            digest.resize(spec.digestBytes);
            return digest;
        }

        std::once_flag m_initFlag;
        bool m_available = false;
        std::mutex m_mutex;

        CComPtr<ID3D11Device> m_device;
        CComPtr<ID3D11DeviceContext> m_context;
        std::array<CComPtr<ID3D11ComputeShader>, KERNEL_COUNT> m_shaders;
        CComPtr<ID3D11Buffer> m_inputBuffer;
        CComPtr<ID3D11ShaderResourceView> m_inputSrv;
        CComPtr<ID3D11Buffer> m_stateBuffer;
        CComPtr<ID3D11UnorderedAccessView> m_stateUav;
        CComPtr<ID3D11Buffer> m_stateStaging;
        CComPtr<ID3D11Buffer> m_paramsBuffer;
    };
}

bool GpuHasher::IsAvailable()
{
    return GpuHashEngine::Get().IsAvailable();
}

std::vector<BYTE> GpuHasher::Hash(const BYTE* data, const size_t size, const HashAlgorithm algo)
{
    return GpuHashEngine::Get().HashBuffer(data, size, algo);
}

std::vector<BYTE> GpuHasher::HashFile(const std::wstring& path, const ULONGLONG sizeLimit,
    const HashAlgorithm algo, const std::function<void()>& progressCallback)
{
    return GpuHashEngine::Get().HashFile(path, sizeLimit, algo, progressCallback);
}
