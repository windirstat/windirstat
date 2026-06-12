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

// GpuHash.hlsl — compute shaders for MD5 (RFC 1321), SHA-1 / SHA-256 /
// SHA-384 / SHA-512 (NIST FIPS 180-4).
//
// Design: hashing is inherently sequential per message, so each dispatch
// runs a single thread that compresses BlockCount consecutive blocks into
// the persistent state buffer. The CPU side performs all message padding,
// so the kernels only ever see whole blocks. SHA-384 reuses the SHA-512
// kernel — the two differ only in their initialization vector and in the
// digest truncation, both of which are handled on the CPU side.
//
// HLSL Shader Model 5.0 has no native 64-bit integer type, therefore the
// SHA-512 kernel emulates 64-bit words as uint2(lo, hi) pairs.

// Message data for this dispatch. Always a whole number of blocks
// (64 bytes for MD5/SHA-1/SHA-256, 128 bytes for SHA-384/512), stored as
// little-endian 32-bit words exactly as they appear in the file.
StructuredBuffer<uint> InputData : register(t0);

// Persistent hash state across dispatches. Layout per algorithm:
//   MD5:     4 words (a, b, c, d)
//   SHA-1:   5 words (h0..h4)
//   SHA-256: 8 words (h0..h7)
//   SHA-512: 16 words (h0.lo, h0.hi, h1.lo, h1.hi, ... h7.lo, h7.hi)
RWStructuredBuffer<uint> HashState : register(u0);

cbuffer HashParams : register(b0)
{
    uint BlockCount; // number of message blocks to process in this dispatch
    uint Reserved0;
    uint Reserved1;
    uint Reserved2;
};

// Reverse the byte order of a 32-bit word. The SHA family interprets
// message bytes as big-endian words while buffer loads are little-endian.
uint ByteSwap(const uint x)
{
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}

uint RotL32(const uint x, const uint n) { return (x << n) | (x >> (32u - n)); }
uint RotR32(const uint x, const uint n) { return (x >> n) | (x << (32u - n)); }

// ===========================================================================
// MD5 (RFC 1321)
// ===========================================================================

// Per-round left-rotation amounts (RFC 1321 section 3.4)
static const uint MD5_S[64] =
{
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

// K[i] = floor(2^32 * abs(sin(i + 1))) (RFC 1321 section 3.4)
static const uint MD5_K[64] =
{
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
    0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
    0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
    0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
};

[numthreads(1, 1, 1)]
void CSMD5(uint3 id : SV_DispatchThreadID)
{
    uint a0 = HashState[0];
    uint b0 = HashState[1];
    uint c0 = HashState[2];
    uint d0 = HashState[3];

    for (uint blk = 0; blk < BlockCount; ++blk)
    {
        // MD5 interprets the message as little-endian words — no swap needed
        uint m[16];
        [unroll]
        for (uint w = 0; w < 16; ++w)
        {
            m[w] = InputData[blk * 16 + w];
        }

        uint a = a0, b = b0, c = c0, d = d0;

        for (uint i = 0; i < 64; ++i)
        {
            uint f, g;
            if (i < 16)      { f = (b & c) | (~b & d);  g = i; }
            else if (i < 32) { f = (d & b) | (~d & c);  g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d;           g = (3 * i + 5) % 16; }
            else             { f = c ^ (b | ~d);        g = (7 * i) % 16; }

            const uint tmp = d;
            d = c;
            c = b;
            b = b + RotL32(a + f + MD5_K[i] + m[g], MD5_S[i]);
            a = tmp;
        }

        a0 += a; b0 += b; c0 += c; d0 += d;
    }

    HashState[0] = a0;
    HashState[1] = b0;
    HashState[2] = c0;
    HashState[3] = d0;
}

// ===========================================================================
// SHA-1 (FIPS 180-4 section 6.1)
// ===========================================================================

[numthreads(1, 1, 1)]
void CSSHA1(uint3 id : SV_DispatchThreadID)
{
    uint h0 = HashState[0];
    uint h1 = HashState[1];
    uint h2 = HashState[2];
    uint h3 = HashState[3];
    uint h4 = HashState[4];

    for (uint blk = 0; blk < BlockCount; ++blk)
    {
        // Message schedule W[0..79] (big-endian words)
        uint w[80];
        [unroll]
        for (uint t = 0; t < 16; ++t)
        {
            w[t] = ByteSwap(InputData[blk * 16 + t]);
        }
        for (uint t2 = 16; t2 < 80; ++t2)
        {
            w[t2] = RotL32(w[t2 - 3] ^ w[t2 - 8] ^ w[t2 - 14] ^ w[t2 - 16], 1);
        }

        uint a = h0, b = h1, c = h2, d = h3, e = h4;

        for (uint i = 0; i < 80; ++i)
        {
            uint f, k;
            if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6u; }

            const uint tmp = RotL32(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = RotL32(b, 30);
            b = a;
            a = tmp;
        }

        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    HashState[0] = h0;
    HashState[1] = h1;
    HashState[2] = h2;
    HashState[3] = h3;
    HashState[4] = h4;
}

// ===========================================================================
// SHA-256 (FIPS 180-4 section 6.2)
// ===========================================================================

// K constants: first 32 bits of the fractional parts of the cube roots
// of the first 64 prime numbers (FIPS 180-4 section 4.2.2)
static const uint SHA256_K[64] =
{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

[numthreads(1, 1, 1)]
void CSSHA256(uint3 id : SV_DispatchThreadID)
{
    uint h[8];
    [unroll]
    for (uint s = 0; s < 8; ++s)
    {
        h[s] = HashState[s];
    }

    for (uint blk = 0; blk < BlockCount; ++blk)
    {
        // Message schedule W[0..63] (big-endian words)
        uint w[64];
        [unroll]
        for (uint t = 0; t < 16; ++t)
        {
            w[t] = ByteSwap(InputData[blk * 16 + t]);
        }
        for (uint t2 = 16; t2 < 64; ++t2)
        {
            const uint s0 = RotR32(w[t2 - 15], 7) ^ RotR32(w[t2 - 15], 18) ^ (w[t2 - 15] >> 3);
            const uint s1 = RotR32(w[t2 - 2], 17) ^ RotR32(w[t2 - 2], 19) ^ (w[t2 - 2] >> 10);
            w[t2] = w[t2 - 16] + s0 + w[t2 - 7] + s1;
        }

        uint a = h[0], b = h[1], c = h[2], d = h[3];
        uint e = h[4], f = h[5], g = h[6], hh = h[7];

        for (uint i = 0; i < 64; ++i)
        {
            const uint S1 = RotR32(e, 6) ^ RotR32(e, 11) ^ RotR32(e, 25);
            const uint ch = (e & f) ^ (~e & g);
            const uint temp1 = hh + S1 + ch + SHA256_K[i] + w[i];
            const uint S0 = RotR32(a, 2) ^ RotR32(a, 13) ^ RotR32(a, 22);
            const uint maj = (a & b) ^ (a & c) ^ (b & c);
            const uint temp2 = S0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    [unroll]
    for (uint s2 = 0; s2 < 8; ++s2)
    {
        HashState[s2] = h[s2];
    }
}

// ===========================================================================
// SHA-512 (FIPS 180-4 section 6.4) — also used for SHA-384, which differs
// only in its initialization vector and digest truncation (CPU side).
//
// 64-bit emulation: a word is uint2(lo, hi). HLSL SM 5.0 has no uint64.
// ===========================================================================

uint2 Add64(const uint2 a, const uint2 b)
{
    const uint lo = a.x + b.x;
    // Carry into the high word when the low word overflowed
    const uint carry = (lo < a.x) ? 1u : 0u;
    return uint2(lo, a.y + b.y + carry);
}

uint2 Xor64(const uint2 a, const uint2 b) { return uint2(a.x ^ b.x, a.y ^ b.y); }
uint2 And64(const uint2 a, const uint2 b) { return uint2(a.x & b.x, a.y & b.y); }
uint2 Not64(const uint2 a)                { return uint2(~a.x, ~a.y); }

// Right-rotate a 64-bit value by n (1..63)
uint2 RotR64(const uint2 v, const uint n)
{
    if (n == 32u)
    {
        return uint2(v.y, v.x);
    }
    if (n < 32u)
    {
        return uint2((v.x >> n) | (v.y << (32u - n)),
                     (v.y >> n) | (v.x << (32u - n)));
    }
    const uint m = n - 32u;
    return uint2((v.y >> m) | (v.x << (32u - m)),
                 (v.x >> m) | (v.y << (32u - m)));
}

// Logical right-shift of a 64-bit value by n (1..31 is all SHA-512 needs)
uint2 ShR64(const uint2 v, const uint n)
{
    return uint2((v.x >> n) | (v.y << (32u - n)), v.y >> n);
}

// K constants: first 64 bits of the fractional parts of the cube roots of
// the first 80 primes (FIPS 180-4 section 4.2.3), stored as (lo, hi)
static const uint2 SHA512_K[80] =
{
    uint2(0xd728ae22u, 0x428a2f98u), uint2(0x23ef65cdu, 0x71374491u),
    uint2(0xec4d3b2fu, 0xb5c0fbcfu), uint2(0x8189dbbcu, 0xe9b5dba5u),
    uint2(0xf348b538u, 0x3956c25bu), uint2(0xb605d019u, 0x59f111f1u),
    uint2(0xaf194f9bu, 0x923f82a4u), uint2(0xda6d8118u, 0xab1c5ed5u),
    uint2(0xa3030242u, 0xd807aa98u), uint2(0x45706fbeu, 0x12835b01u),
    uint2(0x4ee4b28cu, 0x243185beu), uint2(0xd5ffb4e2u, 0x550c7dc3u),
    uint2(0xf27b896fu, 0x72be5d74u), uint2(0x3b1696b1u, 0x80deb1feu),
    uint2(0x25c71235u, 0x9bdc06a7u), uint2(0xcf692694u, 0xc19bf174u),
    uint2(0x9ef14ad2u, 0xe49b69c1u), uint2(0x384f25e3u, 0xefbe4786u),
    uint2(0x8b8cd5b5u, 0x0fc19dc6u), uint2(0x77ac9c65u, 0x240ca1ccu),
    uint2(0x592b0275u, 0x2de92c6fu), uint2(0x6ea6e483u, 0x4a7484aau),
    uint2(0xbd41fbd4u, 0x5cb0a9dcu), uint2(0x831153b5u, 0x76f988dau),
    uint2(0xee66dfabu, 0x983e5152u), uint2(0x2db43210u, 0xa831c66du),
    uint2(0x98fb213fu, 0xb00327c8u), uint2(0xbeef0ee4u, 0xbf597fc7u),
    uint2(0x3da88fc2u, 0xc6e00bf3u), uint2(0x930aa725u, 0xd5a79147u),
    uint2(0xe003826fu, 0x06ca6351u), uint2(0x0a0e6e70u, 0x14292967u),
    uint2(0x46d22ffcu, 0x27b70a85u), uint2(0x5c26c926u, 0x2e1b2138u),
    uint2(0x5ac42aedu, 0x4d2c6dfcu), uint2(0x9d95b3dfu, 0x53380d13u),
    uint2(0x8baf63deu, 0x650a7354u), uint2(0x3c77b2a8u, 0x766a0abbu),
    uint2(0x47edaee6u, 0x81c2c92eu), uint2(0x1482353bu, 0x92722c85u),
    uint2(0x4cf10364u, 0xa2bfe8a1u), uint2(0xbc423001u, 0xa81a664bu),
    uint2(0xd0f89791u, 0xc24b8b70u), uint2(0x0654be30u, 0xc76c51a3u),
    uint2(0xd6ef5218u, 0xd192e819u), uint2(0x5565a910u, 0xd6990624u),
    uint2(0x5771202au, 0xf40e3585u), uint2(0x32bbd1b8u, 0x106aa070u),
    uint2(0xb8d2d0c8u, 0x19a4c116u), uint2(0x5141ab53u, 0x1e376c08u),
    uint2(0xdf8eeb99u, 0x2748774cu), uint2(0xe19b48a8u, 0x34b0bcb5u),
    uint2(0xc5c95a63u, 0x391c0cb3u), uint2(0xe3418acbu, 0x4ed8aa4au),
    uint2(0x7763e373u, 0x5b9cca4fu), uint2(0xd6b2b8a3u, 0x682e6ff3u),
    uint2(0x5defb2fcu, 0x748f82eeu), uint2(0x43172f60u, 0x78a5636fu),
    uint2(0xa1f0ab72u, 0x84c87814u), uint2(0x1a6439ecu, 0x8cc70208u),
    uint2(0x23631e28u, 0x90befffau), uint2(0xde82bde9u, 0xa4506cebu),
    uint2(0xb2c67915u, 0xbef9a3f7u), uint2(0xe372532bu, 0xc67178f2u),
    uint2(0xea26619cu, 0xca273eceu), uint2(0x21c0c207u, 0xd186b8c7u),
    uint2(0xcde0eb1eu, 0xeada7dd6u), uint2(0xee6ed178u, 0xf57d4f7fu),
    uint2(0x72176fbau, 0x06f067aau), uint2(0xa2c898a6u, 0x0a637dc5u),
    uint2(0xbef90daeu, 0x113f9804u), uint2(0x131c471bu, 0x1b710b35u),
    uint2(0x23047d84u, 0x28db77f5u), uint2(0x40c72493u, 0x32caab7bu),
    uint2(0x15c9bebcu, 0x3c9ebe0au), uint2(0x9c100d4cu, 0x431d67c4u),
    uint2(0xcb3e42b6u, 0x4cc5d4beu), uint2(0xfc657e2au, 0x597f299cu),
    uint2(0x3ad6faecu, 0x5fcb6fabu), uint2(0x4a475817u, 0x6c44198cu)
};

[numthreads(1, 1, 1)]
void CSSHA512(uint3 id : SV_DispatchThreadID)
{
    // Load state: HashState[2*i] = lo, HashState[2*i+1] = hi
    uint2 h[8];
    [unroll]
    for (uint s = 0; s < 8; ++s)
    {
        h[s] = uint2(HashState[2 * s], HashState[2 * s + 1]);
    }

    for (uint blk = 0; blk < BlockCount; ++blk)
    {
        // Message schedule W[0..79]. A big-endian 64-bit word at byte
        // offset o consists of the byte-swapped 32-bit word at o (high
        // half) followed by the byte-swapped word at o+4 (low half).
        uint2 w[80];
        [unroll]
        for (uint t = 0; t < 16; ++t)
        {
            const uint hi = ByteSwap(InputData[blk * 32 + 2 * t]);
            const uint lo = ByteSwap(InputData[blk * 32 + 2 * t + 1]);
            w[t] = uint2(lo, hi);
        }
        for (uint t2 = 16; t2 < 80; ++t2)
        {
            // s0 = (w[t-15] rotr 1) xor (w[t-15] rotr 8) xor (w[t-15] shr 7)
            const uint2 s0 = Xor64(Xor64(RotR64(w[t2 - 15], 1), RotR64(w[t2 - 15], 8)), ShR64(w[t2 - 15], 7));
            // s1 = (w[t-2] rotr 19) xor (w[t-2] rotr 61) xor (w[t-2] shr 6)
            const uint2 s1 = Xor64(Xor64(RotR64(w[t2 - 2], 19), RotR64(w[t2 - 2], 61)), ShR64(w[t2 - 2], 6));
            w[t2] = Add64(Add64(w[t2 - 16], s0), Add64(w[t2 - 7], s1));
        }

        uint2 a = h[0], b = h[1], c = h[2], d = h[3];
        uint2 e = h[4], f = h[5], g = h[6], hh = h[7];

        for (uint i = 0; i < 80; ++i)
        {
            // S1 = (e rotr 14) xor (e rotr 18) xor (e rotr 41)
            const uint2 S1 = Xor64(Xor64(RotR64(e, 14), RotR64(e, 18)), RotR64(e, 41));
            const uint2 ch = Xor64(And64(e, f), And64(Not64(e), g));
            const uint2 temp1 = Add64(Add64(Add64(hh, S1), Add64(ch, SHA512_K[i])), w[i]);
            // S0 = (a rotr 28) xor (a rotr 34) xor (a rotr 39)
            const uint2 S0 = Xor64(Xor64(RotR64(a, 28), RotR64(a, 34)), RotR64(a, 39));
            const uint2 maj = Xor64(Xor64(And64(a, b), And64(a, c)), And64(b, c));
            const uint2 temp2 = Add64(S0, maj);

            hh = g;
            g = f;
            f = e;
            e = Add64(d, temp1);
            d = c;
            c = b;
            b = a;
            a = Add64(temp1, temp2);
        }

        h[0] = Add64(h[0], a); h[1] = Add64(h[1], b);
        h[2] = Add64(h[2], c); h[3] = Add64(h[3], d);
        h[4] = Add64(h[4], e); h[5] = Add64(h[5], f);
        h[6] = Add64(h[6], g); h[7] = Add64(h[7], hh);
    }

    [unroll]
    for (uint s2 = 0; s2 < 8; ++s2)
    {
        HashState[2 * s2] = h[s2].x;
        HashState[2 * s2 + 1] = h[s2].y;
    }
}
