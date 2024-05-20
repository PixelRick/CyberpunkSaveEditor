#pragma once
#include <redx/core/platform.hpp>

#include <array>
#include <string>
#include <string_view>

#include <redx/core/utils.hpp>
#include <redx/core/hashing_tables.hpp>

namespace redx {

//--------------------------------------------------------
//  CRC32 (poly:0x04C11DB7, reflected:0xEDB88320)

struct crc32_builder
{
  crc32_builder() = default;

  void init(uint32_t seed = 0)
  {
    m_x = ~seed;
  }

  void update(const void* data, size_t len)
  {
    using detail::crc32::s8lut;
  
    uint32_t x = m_x;

    uint32_t* pd = (uint32_t*)data;
    while (len >= 8)
    {
      const uint32_t a = *pd++ ^ x;
      const uint32_t b = *pd++;
      x = s8lut[7][ a        & 0xFF]
        ^ s8lut[6][(a >>  8) & 0xFF]
        ^ s8lut[5][(a >> 16) & 0xFF]
        ^ s8lut[4][(a >> 24)       ]
        ^ s8lut[3][ b        & 0xFF]
        ^ s8lut[2][(b >>  8) & 0xFF]
        ^ s8lut[1][(b >> 16) & 0xFF]
        ^ s8lut[0][(b >> 24)       ];
      len -= 8;
    }
  
    uint8_t* pc = (uint8_t*)pd;
    while (len--)
    {
      x = (x >> 8) ^ s8lut[0][static_cast<uint8_t>(x ^ *pc++)];
    }

    m_x = x;
  }

  uint32_t finalize()
  {
    return ~m_x;
  }

protected:

  uint32_t m_x = 0;
};

inline uint32_t crc32_bigdata(const char* const data, size_t len, uint32_t seed = 0)
{
  crc32_builder b;
  b.init(seed);
  b.update(data, len);
  return b.finalize();
}

constexpr uint32_t crc32(const char* const data, size_t len, uint32_t seed = 0)
{
  using detail::crc32::s8lut;
  const auto& lut = s8lut[0];

  uint32_t x = ~seed;

  const char* pc = data;
  while (len--)
  {
    x = (x >> 8) ^ s8lut[0][static_cast<uint8_t>(x ^ *pc++)];
  }

  return ~x;
}

constexpr uint32_t crc32_str(std::string_view s, uint32_t seed = 0)
{
  return crc32(s.data(), s.size(), seed);
}

constexpr uint32_t operator""_crc32(const char* const str, std::size_t len)
{
  return crc32_str(std::string_view(str, len));
}

template <typename UIntType>
constexpr UIntType gf2_matrix_times(const std::array<UIntType, sizeof(UIntType) * 8>& mat, UIntType vec)
{
  UIntType sum = 0;
  UIntType i = 0;
  while (vec)
  {
    if (vec & 1)
    {
        sum ^= mat[i];
    }
    vec >>= 1;
    i++;
  }
  return sum;
}

template <typename UIntType>
constexpr void gf2_matrix_square(std::array<UIntType, sizeof(UIntType) * 8>& square, const std::array<UIntType, sizeof(UIntType) * 8>& mat)
{
  for (int n = 0; n < sizeof(UIntType) * 8; n++)
  {
    square[n] = gf2_matrix_times<UIntType>(mat, mat[n]);
  }
}

constexpr uint32_t crc32_combine_bigdata(uint32_t crc1, uint32_t crc2, size_t len2)
{
  if (!len2)
  {
    return crc1;
  }

  // even-power-of-two zeros operator
  std::array<uint32_t, 32> even = {
    0x76DC4190, 0xEDB88320, 0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020,
    0x00000040, 0x00000080, 0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000,
    0x00004000, 0x00008000, 0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000,
    0x00400000, 0x00800000, 0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000
  };

  // odd-power-of-two zeros operator
  std::array<uint32_t, 32> odd = {
    0x1DB71064, 0x3B6E20C8, 0x76DC4190, 0xEDB88320, 0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080, 0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000, 0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000, 0x01000000, 0x02000000, 0x04000000, 0x08000000
  };

  // apply len2 zeros to crc1
  do
  {
      gf2_matrix_square<uint32_t>(even, odd);
      if (len2 & 1)
      {
          crc1 = gf2_matrix_times<uint32_t>(even, crc1);
      }
      len2 >>= 1;

      if (len2 == 0)
      {
          break;
      }

      gf2_matrix_square<uint32_t>(odd, even);
      if (len2 & 1)
      {
          crc1 = gf2_matrix_times<uint32_t>(odd, crc1);
      }
      len2 >>= 1;

  } while (len2 != 0);

  return crc1 ^ crc2;
}

// Tries to use the matrix version only if is worth it.
constexpr uint32_t crc32_combine(uint32_t crc1, uint32_t crc2, size_t len2)
{
  using detail::crc32::s8lut;
  const auto& lut = s8lut[0];

  // bigdata ops ~= 8*32 * k ops * [log2(len2)]
  // current ops ~= k ops * len2
  if (len2 > 3000) // a benchmark would be great 
  {
    return crc32_combine_bigdata(crc1, crc2, len2);
  }

  uint32_t x = crc1;
  while (len2--)
  {
    x = (x >> 8) ^ s8lut[0][static_cast<uint8_t>(x)];
  }

  return x ^ crc2;
}

//--------------------------------------------------------
//  CRC64 (poly:0x42F0E1EBA9EA3693, reflected:0xC96C5795D7870F42)

struct crc64_builder
{
  crc64_builder() = default;

  void init(uint64_t seed = 0)
  {
    m_x = ~seed;
  }

  void update(const void* data, size_t len)
  {
    using detail::crc64::s8lut;
  
    uint64_t x = m_x;

    uint64_t* pd = (uint64_t*)data;
    while (len >= 8)
    {
      const uint64_t a = *pd++ ^ x;
      x = s8lut[7][ a        & 0xFF]
        ^ s8lut[6][(a >>  8) & 0xFF]
        ^ s8lut[5][(a >> 16) & 0xFF]
        ^ s8lut[4][(a >> 24) & 0xFF]
        ^ s8lut[3][(a >> 32) & 0xFF]
        ^ s8lut[2][(a >> 40) & 0xFF]
        ^ s8lut[1][(a >> 48) & 0xFF]
        ^ s8lut[0][(a >> 56)       ];
      len -= 8;
    }
  
    uint8_t* pc = (uint8_t*)pd;
    while (len--)
    {
      x = (x >> 8) ^ s8lut[0][static_cast<uint8_t>(x ^ *pc++)];
    }

    m_x = x;
  }

  uint64_t finalize()
  {
    return ~m_x;
  }

protected:

  uint64_t m_x = 0;
};

inline uint64_t crc64_bigdata(const char* const data, size_t len, uint64_t seed = 0)
{
  crc64_builder b;
  b.init(seed);
  b.update(data, len);
  return b.finalize();
}

constexpr uint64_t crc64(const char* const data, size_t len, uint64_t seed = 0)
{
  using detail::crc64::s8lut;
  const auto& lut = s8lut[0];

  uint64_t x = ~seed;

  const char* pc = data;
  while (len--)
  {
    x = (x >> 8) ^ lut[static_cast<uint8_t>(x ^ *pc++)];
  }

  return ~x;
}

constexpr uint64_t crc64_str(std::string_view s, uint64_t seed = 0)
{
  return crc64(s.data(), s.size(), seed);
}

constexpr uint64_t operator""_crc64(const char* const str, std::size_t len)
{
  return crc64_str(std::string_view(str, len));
}

constexpr uint64_t crc64_combine_bigdata(uint64_t crc1, uint64_t crc2, size_t len2)
{
  if (!len2)
  {
    return crc1;
  }

  // even-power-of-two zeros operator
  std::array<uint64_t, 64> even = {
    0x64B62BCAEBC387A1, 0xC96C5795D7870F42, 0x0000000000000001, 0x0000000000000002, 0x0000000000000004, 0x0000000000000008, 0x0000000000000010, 0x0000000000000020,
    0x0000000000000040, 0x0000000000000080, 0x0000000000000100, 0x0000000000000200, 0x0000000000000400, 0x0000000000000800, 0x0000000000001000, 0x0000000000002000,
    0x0000000000004000, 0x0000000000008000, 0x0000000000010000, 0x0000000000020000, 0x0000000000040000, 0x0000000000080000, 0x0000000000100000, 0x0000000000200000,
    0x0000000000400000, 0x0000000000800000, 0x0000000001000000, 0x0000000002000000, 0x0000000004000000, 0x0000000008000000, 0x0000000010000000, 0x0000000020000000,
    0x0000000040000000, 0x0000000080000000, 0x0000000100000000, 0x0000000200000000, 0x0000000400000000, 0x0000000800000000, 0x0000001000000000, 0x0000002000000000,
    0x0000004000000000, 0x0000008000000000, 0x0000010000000000, 0x0000020000000000, 0x0000040000000000, 0x0000080000000000, 0x0000100000000000, 0x0000200000000000,
    0x0000400000000000, 0x0000800000000000, 0x0001000000000000, 0x0002000000000000, 0x0004000000000000, 0x0008000000000000, 0x0010000000000000, 0x0020000000000000,
    0x0040000000000000, 0x0080000000000000, 0x0100000000000000, 0x0200000000000000, 0x0400000000000000, 0x0800000000000000, 0x1000000000000000, 0x2000000000000000
  };

  // odd-power-of-two zeros operator
  std::array<uint64_t, 64> odd = {
    0x7D9BA13851336649, 0xFB374270A266CC92, 0x64B62BCAEBC387A1, 0xC96C5795D7870F42, 0x0000000000000001, 0x0000000000000002, 0x0000000000000004, 0x0000000000000008,
    0x0000000000000010, 0x0000000000000020, 0x0000000000000040, 0x0000000000000080, 0x0000000000000100, 0x0000000000000200, 0x0000000000000400, 0x0000000000000800,
    0x0000000000001000, 0x0000000000002000, 0x0000000000004000, 0x0000000000008000, 0x0000000000010000, 0x0000000000020000, 0x0000000000040000, 0x0000000000080000,
    0x0000000000100000, 0x0000000000200000, 0x0000000000400000, 0x0000000000800000, 0x0000000001000000, 0x0000000002000000, 0x0000000004000000, 0x0000000008000000,
    0x0000000010000000, 0x0000000020000000, 0x0000000040000000, 0x0000000080000000, 0x0000000100000000, 0x0000000200000000, 0x0000000400000000, 0x0000000800000000,
    0x0000001000000000, 0x0000002000000000, 0x0000004000000000, 0x0000008000000000, 0x0000010000000000, 0x0000020000000000, 0x0000040000000000, 0x0000080000000000,
    0x0000100000000000, 0x0000200000000000, 0x0000400000000000, 0x0000800000000000, 0x0001000000000000, 0x0002000000000000, 0x0004000000000000, 0x0008000000000000,
    0x0010000000000000, 0x0020000000000000, 0x0040000000000000, 0x0080000000000000, 0x0100000000000000, 0x0200000000000000, 0x0400000000000000, 0x0800000000000000,
  };

  // apply len2 zeros to crc1
  do
  {
      gf2_matrix_square<uint64_t>(even, odd);
      if (len2 & 1)
          crc1 = gf2_matrix_times<uint64_t>(even, crc1);
      len2 >>= 1;

      if (len2 == 0)
          break;

      gf2_matrix_square<uint64_t>(odd, even);
      if (len2 & 1)
          crc1 = gf2_matrix_times<uint64_t>(odd, crc1);
      len2 >>= 1;

  } while (len2 != 0);

  return crc1 ^ crc2;
}

// Tries to use the matrix version only if is worth it.
constexpr uint64_t crc64_combine(uint64_t crc1, uint64_t crc2, size_t len2)
{
  using detail::crc64::s8lut;
  const auto& lut = s8lut[0];

  // bigdata ops ~= 8*64 * k ops * [log2(len2)]
  // current ops ~= k ops * len2
  if (len2 > 6500) // a benchmark would be great 
  {
    return crc64_combine_bigdata(crc1, crc2, len2);
  }

  uint64_t x = crc1;
  while (len2--)
  {
    x = (x >> 8) ^ s8lut[0][static_cast<uint8_t>(x)];
  }

  return x ^ crc2;
}

//--------------------------------------------------------
//  FNV1A32

// TODO: check that it gets computed at compile-time in every case for which it can !
//       for instance "text"_fnv1a32 was translated to a call when used as an argument instead of assignment..

constexpr uint32_t fnv1a32_continue(uint32_t hash, std::string_view str)
{
  constexpr uint32_t prime = 0x01000193;

  for (auto c : str)
  {
    hash ^= c;
    hash *= prime;
  }

  return hash;
}

constexpr uint32_t fnv1a32(std::string_view str)
{
  constexpr uint32_t basis = 0x811C9DC5;

  return fnv1a32_continue(basis, str);
}

constexpr uint32_t operator""_fnv1a32(const char* const str, std::size_t len)
{
  return fnv1a32(std::string_view(str, len));
}

//--------------------------------------------------------
//  FNV1A64

using fnv1a64_t = uint64_t;

constexpr fnv1a64_t fnv1a64_continue(fnv1a64_t hash, std::string_view str)
{
  constexpr uint64_t prime = 0x00000100000001B3;

  uint64_t x = uint64_t(hash);
  for (auto c : str)
  {
    x ^= c;
    x *= prime;
  }

  return {x};
}

constexpr fnv1a64_t fnv1a64(std::string_view str)
{
  constexpr uint64_t basis = 0xCBF29CE484222325;

  return fnv1a64_continue(basis, str);
}

constexpr fnv1a64_t operator""_fnv1a64(const char* const str, std::size_t len)
{
  return fnv1a64(std::string_view(str, len));
}

//--------------------------------------------------------
//  MURMUR3_32

namespace detail::murmur3_32 {

constexpr uint32_t fmix32(uint32_t h)
{
  h ^= h >> 16;
  h *= 0x85EBCA6B;
  h ^= h >> 13;
  h *= 0xC2B2AE35;
  h ^= h >> 16;
  return h;
}

constexpr uint32_t get_block(const char* const data, size_t i)
{
  const size_t j = i * 4;
  return (uint8_t)data[j] | ((uint8_t)data[j + 1] << 8) | ((uint8_t)data[j + 2] << 16) | ((uint8_t)data[j + 3] << 24);
}

} // namespace detail::murmur3_32


constexpr uint32_t murmur3_32(const char* const data, size_t len, uint32_t seed = 0x5EEDBA5E)
{
  using namespace detail::murmur3_32;

  const uint32_t size = static_cast<uint32_t>(len);
  const size_t nblocks = size / 4;

  uint32_t h1 = seed;

  const uint32_t c1 = 0xCC9E2D51;
  const uint32_t c2 = 0x1B873593;

  //----------
  // body

  for (size_t i = 0; i < nblocks; ++i)
  {
    uint32_t k1 = get_block(data, i);

    k1 *= c1;
    k1 = rol(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = rol(h1, 13);
    h1 = h1 * 5 + 0xE6546B64;
  }

  //----------
  // tail

  const char* tail = data + nblocks * 4;

  uint32_t k1 = 0;

  switch (size & 3)
  {
    case 3:
      k1 ^= (uint8_t)tail[2] << 16;
      [[fallthrough]];
    case 2:
      k1 ^= (uint8_t)tail[1] << 8;
      [[fallthrough]];
    case 1:
      k1 ^= (uint8_t)tail[0];
      k1 *= c1;
      k1 = rol(k1, 15);
      k1 *= c2;
      h1 ^= k1;
      break;
    default:
      break;
  };

  //----------
  // finalization

  h1 ^= size;

  return fmix32(h1);
}

constexpr uint32_t murmur3_32(std::string_view s, uint32_t seed = 0x5EEDBA5E)
{
  return murmur3_32(s.data(), s.size(), seed);
}

constexpr uint32_t operator""_murmur3_32(const char* str, std::size_t len)
{
  return murmur3_32(std::string_view(str, len));
}

//--------------------------------------------------------
//  SHA1

namespace detail::sha1 {

constexpr uint32_t blk(uint32_t block[16], size_t i)
{
  return rol(
    block[(i + 13) & 0xF] ^
    block[(i +  8) & 0xF] ^
    block[(i +  2) & 0xF] ^
    block[i],
    1);
}

inline void r0(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  z += 0x5A827999 + block[i] + rol(v, 5) + ((w & (x ^ y)) ^ y) ;
  w = rol(w, 30);
}

inline void r1(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0x5A827999 + rol(v, 5) + ((w & (x ^ y)) ^ y);
  w = rol(w, 30);
}

inline void r2(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0x6ED9EBA1 + rol(v, 5) + (w ^ x ^ y);
  w = rol(w, 30);
}

inline void r3(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0x8F1BBCDC + rol(v, 5) + (((w | x) & y) | (w & x));
  w = rol(w, 30);
}

inline void r4(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0xCA62C1D6 + rol(v, 5) + (w ^ x ^ y);
  w = rol(w, 30);
}

inline void make_block(uint8_t const* pb, uint32_t block[16])
{
  for (size_t i = 0; i < 16; i++)
  {
    block[i] =
        (static_cast<uint32_t>(pb[4 * i + 3]))
      | (static_cast<uint32_t>(pb[4 * i + 2])) <<  8
      | (static_cast<uint32_t>(pb[4 * i + 1])) << 16
      | (static_cast<uint32_t>(pb[4 * i + 0])) << 24;
  }
}

} // namespace detail::sha1


struct sha1_digest
{
  uint32_t parts[5];
};

struct sha1_builder
{
  void init()
  {
    m_cb = 0;
    m_len = 0;
    m_digest[0] = 0x67452301;
    m_digest[1] = 0xEFCDAB89;
    m_digest[2] = 0x98BADCFE;
    m_digest[3] = 0x10325476;
    m_digest[4] = 0xC3D2E1F0;
  }

  void update(const char* data, size_t len)
  {
    using namespace detail::sha1;

    while (1)
    {
      const uint32_t n = (uint32_t)(std::min)(len, sizeof(m_buf) - m_len);
      memcpy(m_buf + m_len, data, n);
      m_len += n;
      if (m_len != 64)
      {
        return;
      }

      data += n;
      len -= n;
      m_len = 0;

      uint32_t block[16];
      make_block(m_buf, block);
      transform(m_digest, block);
      ++m_cb;
    }
  }

  sha1_digest finalize()
  {
    using namespace detail::sha1;
    sha1_digest out;

    uint64_t total_bits = (m_cb * 64 + m_len) * 8;

    m_buf[m_len++] = 0x80;
    memset(m_buf + m_len, 0, sizeof(m_buf) - m_len);

    uint32_t block[16];
    make_block(m_buf, block);

    if (m_len > 56)
    {
        transform(m_digest, block);
        for (size_t i = 0; i < 16 - 2; i++)
        {
            block[i] = 0;
        }
    }

    block[16 - 1] = total_bits & 0xFFFFFFFF;
    block[16 - 2] = (total_bits >> 32);
    transform(m_digest, block);

    for (size_t i = 0; i < 5; i++)
    {
        out.parts[i] = byteswap(m_digest[i]);
    }

    return out;
  }

protected:

  void transform(uint32_t digest[5], uint32_t block[16])
  {
    using namespace detail::sha1;

    uint32_t a = digest[0];
    uint32_t b = digest[1];
    uint32_t c = digest[2];
    uint32_t d = digest[3];
    uint32_t e = digest[4];
  
    r0(block, a, b, c, d, e,  0);
    r0(block, e, a, b, c, d,  1);
    r0(block, d, e, a, b, c,  2);
    r0(block, c, d, e, a, b,  3);
    r0(block, b, c, d, e, a,  4);
    r0(block, a, b, c, d, e,  5);
    r0(block, e, a, b, c, d,  6);
    r0(block, d, e, a, b, c,  7);
    r0(block, c, d, e, a, b,  8);
    r0(block, b, c, d, e, a,  9);
    r0(block, a, b, c, d, e, 10);
    r0(block, e, a, b, c, d, 11);
    r0(block, d, e, a, b, c, 12);
    r0(block, c, d, e, a, b, 13);
    r0(block, b, c, d, e, a, 14);
    r0(block, a, b, c, d, e, 15);
    r1(block, e, a, b, c, d,  0);
    r1(block, d, e, a, b, c,  1);
    r1(block, c, d, e, a, b,  2);
    r1(block, b, c, d, e, a,  3);
    r2(block, a, b, c, d, e,  4);
    r2(block, e, a, b, c, d,  5);
    r2(block, d, e, a, b, c,  6);
    r2(block, c, d, e, a, b,  7);
    r2(block, b, c, d, e, a,  8);
    r2(block, a, b, c, d, e,  9);
    r2(block, e, a, b, c, d, 10);
    r2(block, d, e, a, b, c, 11);
    r2(block, c, d, e, a, b, 12);
    r2(block, b, c, d, e, a, 13);
    r2(block, a, b, c, d, e, 14);
    r2(block, e, a, b, c, d, 15);
    r2(block, d, e, a, b, c,  0);
    r2(block, c, d, e, a, b,  1);
    r2(block, b, c, d, e, a,  2);
    r2(block, a, b, c, d, e,  3);
    r2(block, e, a, b, c, d,  4);
    r2(block, d, e, a, b, c,  5);
    r2(block, c, d, e, a, b,  6);
    r2(block, b, c, d, e, a,  7);
    r3(block, a, b, c, d, e,  8);
    r3(block, e, a, b, c, d,  9);
    r3(block, d, e, a, b, c, 10);
    r3(block, c, d, e, a, b, 11);
    r3(block, b, c, d, e, a, 12);
    r3(block, a, b, c, d, e, 13);
    r3(block, e, a, b, c, d, 14);
    r3(block, d, e, a, b, c, 15);
    r3(block, c, d, e, a, b,  0);
    r3(block, b, c, d, e, a,  1);
    r3(block, a, b, c, d, e,  2);
    r3(block, e, a, b, c, d,  3);
    r3(block, d, e, a, b, c,  4);
    r3(block, c, d, e, a, b,  5);
    r3(block, b, c, d, e, a,  6);
    r3(block, a, b, c, d, e,  7);
    r3(block, e, a, b, c, d,  8);
    r3(block, d, e, a, b, c,  9);
    r3(block, c, d, e, a, b, 10);
    r3(block, b, c, d, e, a, 11);
    r4(block, a, b, c, d, e, 12);
    r4(block, e, a, b, c, d, 13);
    r4(block, d, e, a, b, c, 14);
    r4(block, c, d, e, a, b, 15);
    r4(block, b, c, d, e, a,  0);
    r4(block, a, b, c, d, e,  1);
    r4(block, e, a, b, c, d,  2);
    r4(block, d, e, a, b, c,  3);
    r4(block, c, d, e, a, b,  4);
    r4(block, b, c, d, e, a,  5);
    r4(block, a, b, c, d, e,  6);
    r4(block, e, a, b, c, d,  7);
    r4(block, d, e, a, b, c,  8);
    r4(block, c, d, e, a, b,  9);
    r4(block, b, c, d, e, a, 10);
    r4(block, a, b, c, d, e, 11);
    r4(block, e, a, b, c, d, 12);
    r4(block, d, e, a, b, c, 13);
    r4(block, c, d, e, a, b, 14);
    r4(block, b, c, d, e, a, 15);
  
    digest[0] += a;
    digest[1] += b;
    digest[2] += c;
    digest[3] += d;
    digest[4] += e;
  }

protected:

  uint64_t  m_cb = 0;
  uint32_t  m_digest[5]{};
  uint32_t  m_len = 0;
  uint8_t   m_buf[64]{};
};

inline sha1_digest sha1(const char* const data, size_t len)
{
  sha1_builder b;
  b.init();
  b.update(data, len);
  return b.finalize();
}

FORCE_INLINE sha1_digest sha1(std::string_view s)
{
  return sha1(s.data(), s.size());
}

//--------------------------------------------------------
//  CHECKS

static_assert(0xE8F35A06 == "testing"_crc32, "constexpr crc32 failed");
static_assert(0xC032DDBA == "somestring"_crc32, "constexpr crc32 failed");
static_assert(0x6BFB9DCC == "testingsomestring"_crc32, "constexpr crc32 failed");
static_assert(0x6BFB9DCC == crc32("somestring", 10, 0xE8F35A06), "constexpr crc32 with seed failed");
static_assert(0x6BFB9DCC == crc32_combine(0xE8F35A06, 0xC032DDBA, 10), "constexpr crc32_combine failed");
static_assert(0x6BFB9DCC == crc32_combine_bigdata(0xE8F35A06, 0xC032DDBA, 10), "constexpr crc32_combine_bigdata failed");

static_assert(0x0C12B945415B900F == "testing"_crc64, "constexpr crc64 failed");
static_assert(0xFAEC849CAC48EAF1 == "somestring"_crc64, "constexpr crc64 failed");
static_assert(0x8463A431DE14344F == "testingsomestring"_crc64, "constexpr crc64 failed");
static_assert(0x8463A431DE14344F == crc64("somestring", 10, 0x0C12B945415B900F), "constexpr crc64 with seed failed");
static_assert(0x8463A431DE14344F == crc64_combine(0x0C12B945415B900F, 0xFAEC849CAC48EAF1, 10), "constexpr crc64_combine failed");
static_assert(0x8463A431DE14344F == crc64_combine_bigdata(0x0C12B945415B900F, 0xFAEC849CAC48EAF1, 10), "constexpr crc64_combine_bigdata failed");

static_assert(0xEB5F499B == "testing"_fnv1a32, "constexpr fnv1a32 failed");
static_assert(0xC2FE2FB77AE839BB == "testing"_fnv1a64, "constexpr fnv1a64 failed");
static_assert(0xC5FC3C78 == "testing"_murmur3_32, "constexpr murmur3_32 failed");

} // namespace redx

