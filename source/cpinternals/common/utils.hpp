#pragma once
#include <inttypes.h>
#include <intrin.h>
#include <vector>

#if __has_include(<span>) && (!defined(_HAS_CXX20) or _HAS_CXX20)
#include <span>
#else
#include <span.hpp>
namespace std { using tcb::span; }
#endif

namespace cp {

inline bool starts_with(const std::string& str, const std::string& with)
{
  return with.length() <= str.length()
    && std::equal(with.begin(), with.end(), str.begin());
}

template <typename T>
constexpr T ror(T x, int16_t n)
{
  if (n == 0)
    return x;
  const uint8_t nbits = sizeof(T) * 8;
  const auto ux = static_cast<std::make_unsigned<T>::type>(x);
  while (n < 0)
    n += nbits;
  n %= nbits;
  return (ux >> n) | (ux << (nbits - n));
}

template <typename T>
constexpr T rol(T x, int16_t n)
{
  if (n == 0)
    return x;
  const uint8_t nbits = sizeof(T) * 8;
  const auto ux = static_cast<std::make_unsigned<T>::type>(x);
  while (n < 0)
    n += nbits;
  n %= nbits;
  return (ux << n) | (ux >> (nbits - n));
}

template <typename T, std::enable_if_t<sizeof(T) <= 4, int> = 0>
inline unsigned ctz(const T value)
{
  unsigned long trailing_zero = 0;
  return _BitScanForward(&trailing_zero, value) ? trailing_zero : sizeof(T) * 8;
}

template <int LSB, int Size, typename T>
T read_bitfield(T& v) 
{
	return (v >> LSB) & ((1 << Size) - 1);
} 

template <int LSB, typename T>
T read_bitfield(T& v) 
{
	return read_bitfield<LSB, 1>(v);
}

constexpr uint16_t byteswap(uint16_t value) noexcept
{
	return (value << 8) | (value >> 8);
}

constexpr uint32_t byteswap(uint32_t value) noexcept
{
	uint32_t tmp = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0x00FF00FF);
	return (tmp << 16) | (tmp >> 16);
}

template <typename T>
typename std::vector<T>::iterator 
insert_sorted(std::vector<T>& vec, const T& item)
{
	return vec.insert( 
		std::lower_bound(vec.begin(), vec.end(), item),
		item);
}

template <typename T>
std::pair<typename std::vector<T>::iterator, bool>
insert_sorted_nodupe(std::vector<T>& vec, const T& item)
{
	auto it = std::lower_bound(vec.begin(), vec.end(), item);
	if (it != vec.end() && *it == item)
		return std::make_pair(it, false);
	return std::make_pair(vec.insert(it, item), true);
}

template <typename T>
std::pair<typename std::vector<T>::iterator, bool>
insert_sorted_nodupe(std::vector<T>& vec, typename std::vector<T>::iterator start, const T& item)
{
	auto it = std::lower_bound(start, vec.end(), item);
	if (it != vec.end() && *it == item)
		return std::make_pair(it, false);
	return std::make_pair(vec.insert(it, item), true);
}

std::vector<uintptr_t> sse2_strstr_masked(uintptr_t hs, size_t m, const uint8_t* needle, size_t n, const char* mask, size_t maxcnt = 0);
std::vector<uintptr_t> sse2_strstr(uintptr_t hs, size_t m, const uint8_t* needle, size_t n, size_t maxcnt = 0);

inline std::vector<uintptr_t> sse2_strstr_masked(const void* hs, size_t m, const uint8_t* needle, size_t n, const char* mask, size_t maxcnt = 0)
{
  return sse2_strstr_masked((uintptr_t)hs, m, needle, n, mask, maxcnt);
}

inline std::vector<uintptr_t> sse2_strstr(const void* hs, size_t m, const uint8_t* needle, size_t n, size_t maxcnt = 0)
{
  return sse2_strstr((uintptr_t)hs, m, needle, n, maxcnt);
}

} // namespace cp

