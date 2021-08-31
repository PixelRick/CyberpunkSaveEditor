#pragma once
#include <inttypes.h>
#include <intrin.h>
#include <vector>
#include <algorithm>
#include <type_traits>

#if __has_include(<span>) && (!defined(_HAS_CXX20) or _HAS_CXX20)
#include <span>
#else
#include <span.hpp>
namespace std { using tcb::span; }
#endif

#include <spdlog/spdlog.h>

namespace redx {

struct nop_mutex
{
  nop_mutex(const nop_mutex&) = delete;
  nop_mutex& operator=(const nop_mutex&) = delete;

  void lock() {}
  void try_lock() {}
  void unlock() {}
  void lock_shared() {}
  void try_lock_shared() {}
  void unlock_shared() {}
};

inline bool starts_with(const std::string& str, const std::string& with)
{
  return with.length() <= str.length()
    && std::equal(with.begin(), with.end(), str.begin());
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
struct integer_range
{
  using integer_type = T;

  integer_range() = default;

  // if end < beg, then end == beg and a warning is logged
  integer_range(integer_type beg, integer_type end)
    : m_beg(beg)
    , m_end(end)
  {
    if (m_end < m_beg)
    {
      SPDLOG_WARN("m_end < m_beg");
      m_end = m_beg;
    }
  }

  integer_type size() const
  {
    return m_end - m_beg;
  }

  bool empty() const
  {
    return m_end == m_beg;
  }

  integer_type beg() const
  {
    return m_beg;
  }

  integer_type end() const
  {
    return m_end;
  }

  integer_type operator[](integer_type idx) const
  {
    if (idx > size())
    {
      SPDLOG_ERROR("idx > size()");
      return m_end;
    }

    return m_beg + idx;
  }

  integer_range offset(integer_type offset)
  {
    // todo: add overflow checks
    return {m_beg + offset, m_end + offset};
  }

  bool is_subrange(integer_range other) const
  {
    return (other.beg() >= m_beg) && (other.beg() <= other.end()) && (other.end() <= m_end);
  }

  bool is_valid_subrange(integer_type offset, integer_type count = integer_type(-1)) const
  {
    return is_subrange( subrange_unchecked(offset, count) );
  }

  integer_range subrange(integer_type offset, integer_type count = integer_type(-1)) const
  {
    const integer_range ret = subrange_unchecked(offset, count);

    if (!is_subrange(ret))
    {
      SPDLOG_ERROR("invalid parameters");
      return integer_range();
    }

    return ret;
  }

  template <typename T>
  std::span<T>
  slice(std::vector<T>& v) const
  {
    const auto& it = v.begin() + m_beg;
    return std::span<T>(&*it, &*it + size());
  }

  template <typename T>
  std::span<const T>
  slice(const std::vector<T>& v) const
  {
    const auto& it = v.begin() + m_beg;
    return std::span<const T>(&*it, &*it + size());
  }

protected:

  integer_range subrange_unchecked(integer_type offset, integer_type count = integer_type(-1)) const
  {
    const integer_type beg_index = m_beg + offset;
    const integer_type end_index = (count == integer_type(-1)) ? m_end : m_beg + offset + count;

    return integer_range(beg_index, end_index);
  }

private:

  integer_type m_beg = 0;
  integer_type m_end = 0;
};

using u32range = integer_range<uint32_t>;
using u64range = integer_range<uint64_t>;
using i32range = integer_range<int32_t>;
using i64range = integer_range<int64_t>;

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

} // namespace redx

