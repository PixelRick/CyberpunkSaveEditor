#pragma once
#include <inttypes.h>
#include <array>
#include <string>

namespace cp {

constexpr uint32_t crc32(const char* const data, size_t len, uint32_t seed = 0)
{
	constexpr std::array<uint32_t, 16> lut = {
		0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
		0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
	};

	uint32_t x = ~seed;
	for (size_t i = 0; i < len; ++i)
	{
    const uint8_t b = static_cast<uint8_t>(data[i]);
		x = lut[(x ^ b) & 0x0F] ^ (x >> 4);
		x = lut[(x ^ (b >> 4)) & 0x0F] ^ (x >> 4);
	};
	return ~x;
}

constexpr uint32_t crc32(std::string_view s, uint32_t seed = 0)
{
	return crc32(s.data(), s.size(), seed);
}

constexpr uint32_t operator""_crc32(const char* const str, std::size_t len)
{
	return crc32(std::string_view(str, len));
}


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

constexpr uint32_t rotl32(uint32_t x, int8_t r)
{
  return (x << r) | (x >> (32 - r));
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

static_assert(0xE8F35A06 == "testing"_crc32, "constexpr crc32 failed");
static_assert(0xEB5F499B == "testing"_fnv1a32, "constexpr fnv1a32 failed");
static_assert(0xC2FE2FB77AE839BB == "testing"_fnv1a64, "constexpr fnv1a64 failed");
static_assert(0xC5FC3C78 == "testing"_murmur3_32, "constexpr murmur3_32 failed");

} // namespace cp

