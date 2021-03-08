#pragma once
#include <inttypes.h>
#include <array>
#include <string>
#include <cpinternals/common/utils.hpp>

namespace cp {

//--------------------------------------------------------
//  CRC32

namespace detail::crc32 {

constexpr static std::array<uint32_t, 16> lut = {
  0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
  0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

} // namespace detail::crc32

constexpr uint32_t crc32(const char* const data, size_t len, uint32_t seed = 0)
{
	using detail::crc32::lut;

	uint32_t x = ~seed;
	for (size_t i = 0; i < len; ++i)
	{
    const uint8_t b = static_cast<uint8_t>(data[i]);
		x = lut[(x ^ b) & 0x0F] ^ (x >> 4);
		x = lut[(x ^ (b >> 4)) & 0x0F] ^ (x >> 4);
	}
	return ~x;
}

// This m_ver isn't using the matrix trick and should be faster for small values of len.
constexpr uint32_t crc32_small_combine(uint32_t a, uint32_t b, size_t len)
{
  using detail::crc32::lut;

  uint32_t x = a;
  for (size_t i = 0; i < len; ++i)
  {
    x = lut[x & 0x0F] ^ (x >> 4);
    x = lut[x & 0x0F] ^ (x >> 4);
  }
  return x ^ b;
}

constexpr uint32_t crc32(std::string_view s, uint32_t seed = 0)
{
	return crc32(s.data(), s.size(), seed);
}

constexpr uint32_t operator""_crc32(const char* const str, std::size_t len)
{
	return crc32(std::string_view(str, len));
}

//--------------------------------------------------------
//  FNV1A32

constexpr uint32_t fnv1a32(std::string_view str)
{
	constexpr uint32_t basis = 0X811C9DC5;
	constexpr uint32_t prime = 0x01000193;

	uint32_t hash = basis;
	for (auto c : str)
	{
		hash ^= c;
		hash *= prime;
	}

	return hash;
}

constexpr uint32_t operator""_fnv1a32(const char* const str, std::size_t len)
{
	return fnv1a32(std::string_view(str, len));
}

//--------------------------------------------------------
//  FNV1A64

constexpr uint64_t fnv1a64(std::string_view str)
{
	constexpr uint64_t basis = 0xCBF29CE484222325;
	constexpr uint64_t prime = 0x00000100000001B3;

	uint64_t hash = basis;
	for (auto c : str)
	{
		hash ^= c;
		hash *= prime;
	}

	return hash;
}

constexpr uint64_t operator""_fnv1a64(const char* const str, std::size_t len)
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
    k1 = rotl32(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = rotl32(h1, 13);
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
    case 2:
      k1 ^= (uint8_t)tail[1] << 8;
    case 1:
      k1 ^= (uint8_t)tail[0];
      k1 *= c1;
      k1 = rotl32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
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
  return rotl32(
    block[(i + 13) & 0xF] ^
    block[(i +  8) & 0xF] ^
    block[(i +  2) & 0xF] ^
    block[i],
    1);
}

inline void r0(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  z += 0x5A827999 + block[i] + rotl32(v, 5) + ((w & (x ^ y)) ^ y) ;
  w = rotl32(w, 30);
}

inline void r1(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0x5A827999 + rotl32(v, 5) + ((w & (x ^ y)) ^ y);
  w = rotl32(w, 30);
}

inline void r2(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0x6ED9EBA1 + rotl32(v, 5) + (w ^ x ^ y);
  w = rotl32(w, 30);
}

inline void r3(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0x8F1BBCDC + rotl32(v, 5) + (((w | x) & y) | (w & x));
  w = rotl32(w, 30);
}

inline void r4(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, std::size_t i)
{
  block[i] = blk(block, i);
  z += block[i] + 0xCA62C1D6 + rotl32(v, 5) + (w ^ x ^ y);
  w = rotl32(w, 30);
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
      const uint32_t n = (uint32_t)std::min(len, sizeof(m_buf) - m_len);
      memcpy(m_buf + m_len, data, n);
      m_len += n;
      if (m_len != 64)
        return;

      data += n;
      len -= n;
      m_len = 0;

      uint32_t block[16];
      make_block(m_buf, block);
      transform(m_digest, block);
      ++m_cb;
    }
  }

  sha1_digest final()
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

  uint64_t  m_cb;
  uint32_t  m_digest[5];
  uint32_t  m_len;
  uint8_t   m_buf[64];
};

inline sha1_digest sha1(const char* const data, size_t len)
{
  sha1_builder b;
  b.init();
  b.update(data, len);
  return b.final();
}

inline sha1_digest sha1(std::string_view s)
{
  return sha1(s.data(), s.size());
}

//--------------------------------------------------------
//  CHECKS

static_assert(0xE8F35A06 == "testing"_crc32, "constexpr crc32 failed");
static_assert(0x3A6907F7 == crc32("testing", 7, 0xE8F35A06), "constexpr crc32 with seed failed");
static_assert(0x3A6907F7 == crc32_small_combine(0xE8F35A06, 0xE8F35A06, 7), "constexpr crc32_small_combine failed");

static_assert(0xEB5F499B == "testing"_fnv1a32, "constexpr fnv1a32 failed");
static_assert(0xC2FE2FB77AE839BB == "testing"_fnv1a64, "constexpr fnv1a64 failed");
static_assert(0xC5FC3C78 == "testing"_murmur3_32, "constexpr murmur3_32 failed");

} // namespace cp

