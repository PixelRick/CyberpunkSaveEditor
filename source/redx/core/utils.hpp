#pragma once
#include <redx/core/platform.hpp>

#include <intrin.h>
#include <vector>
#include <string>
#include <algorithm>
#include <type_traits>
#include <limits>

#if __has_include(<span>) && (!defined(_HAS_CXX20) or _HAS_CXX20)
#include <span>
#else
#include <span.hpp>
namespace std { using tcb::span; }
#endif

namespace redx {

template <typename T>
struct type_identity
{
  using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

template <typename T>
struct remove_cvref
{
  typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

template <typename T, class = void>
constexpr inline bool is_iterator_v = false;

template <typename T>
constexpr inline bool is_iterator_v<T, std::void_t<typename std::iterator_traits<T>::iterator_category>> = true;


template <typename To, typename From>
class is_unsafe_numeric_cast
{
  using tgt_type = std::remove_cv_t<std::remove_reference_t<To>>;
  using src_type = std::remove_cv_t<std::remove_reference_t<From>>;

public:

  static constexpr bool value = std::conjunction_v<
    std::bool_constant<std::is_signed_v<tgt_type> != std::is_signed_v<src_type>>,
    std::bool_constant<((std::numeric_limits<std::make_unsigned_t<tgt_type>>::max)() < (std::numeric_limits<std::make_unsigned_t<src_type>>::max)())>>;
};

template <typename To, typename From>
constexpr inline bool is_unsafe_numeric_cast_v = is_unsafe_numeric_cast<To, From>::value;

// this one checks for overflow in debug mode only
template <typename T, typename U>
FORCE_INLINE T reliable_numeric_cast(U x)
{
  auto ret = static_cast<T>(x);

#ifdef _DEBUG
  if constexpr (is_unsafe_numeric_cast_v<T, U>)
  {
    if (static_cast<U>(ret) != x)
    {
      SPDLOG_CRITICAL("numeric_cast error {} -> {}", x, ret);
      DEBUG_BREAK();
    }

  }
#endif

  return ret;
}

template <typename T, typename U>
FORCE_INLINE T numeric_cast(U x)
{
  auto ret = static_cast<T>(x);

  if constexpr (is_unsafe_numeric_cast_v<T, U>)
  {
    if (static_cast<U>(ret) != x)
    {
      SPDLOG_CRITICAL("numeric_cast error {} -> {}", x, ret);
      DEBUG_BREAK();
    }
  }

  return ret;
}

template <typename T, typename U>
FORCE_INLINE T numeric_cast(U x, bool& ok)
{
  auto ret = static_cast<T>(x);

  if constexpr (is_unsafe_numeric_cast_v<T, U>)
  {
    if (static_cast<U>(ret) != x)
    {
      SPDLOG_ERROR("numeric_cast error {} -> {}", x, ret);
      ok = false;
    }
  }

  ok = true;
  return ret;
}


// alignment is assumed to be a power of two
FORCE_INLINE constexpr uintptr_t align_up(uintptr_t address, size_t alignment)
{
  return (address + alignment - 1) & ~(alignment - 1);
}

// alignment is assumed to be a power of two
FORCE_INLINE constexpr void* align_up(void* address, size_t alignment)
{
  return (void*)(align_up(reinterpret_cast<uintptr_t>(address), alignment));
}

// alignment is assumed to be a power of two
FORCE_INLINE constexpr uintptr_t align_down(uintptr_t address, size_t alignment)
{
  return (address) & ~(alignment - 1);
}

// alignment is assumed to be a power of two
FORCE_INLINE constexpr void* align_down(void* address, size_t alignment)
{
  return (void*)(align_down(reinterpret_cast<uintptr_t>(address), alignment));
}

template <size_t alignment>
FORCE_INLINE constexpr void* align_up(void* address)
{
  static_assert((alignment & (alignment - 1)) == 0, "alignment must be a power of two");
  return (void*)(((uintptr_t)address + alignment - 1) & ~(alignment - 1));
}


struct nop_mutex
{
  nop_mutex(const nop_mutex&) = delete;
  nop_mutex& operator=(const nop_mutex&) = delete;

  FORCE_INLINE void lock() const noexcept {}
  FORCE_INLINE void try_lock() const noexcept {}
  FORCE_INLINE void unlock() const noexcept {}
  FORCE_INLINE void lock_shared() const noexcept {}
  FORCE_INLINE void try_lock_shared() const noexcept {}
  FORCE_INLINE void unlock_shared() const noexcept {}
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

  integer_range() noexcept = default;

  // if end < beg, then end == beg and a warning is logged
  integer_range(integer_type beg, integer_type end) noexcept
    : m_beg(beg)
    , m_end(end)
  {
    if (m_end < m_beg)
    {
      SPDLOG_WARN("m_end < m_beg");
      m_end = m_beg;
    }
  }

  FORCE_INLINE integer_type size() const noexcept
  {
    return m_end - m_beg;
  }

  FORCE_INLINE bool empty() const noexcept
  {
    return m_end == m_beg;
  }

  FORCE_INLINE integer_type beg() const noexcept
  {
    return m_beg;
  }

  FORCE_INLINE integer_type end() const noexcept
  {
    return m_end;
  }

  inline integer_type operator[](integer_type idx) const noexcept
  {
    if (idx > size())
    {
      SPDLOG_ERROR("idx > size()");
      return m_end;
    }

    return m_beg + idx;
  }

  FORCE_INLINE integer_range offset(integer_type offset) noexcept
  {
    // todo: add overflow checks
    return {m_beg + offset, m_end + offset};
  }

  bool is_subrange(integer_range other) const noexcept
  {
    return (other.beg() >= m_beg) && (other.beg() <= other.end()) && (other.end() <= m_end);
  }

  bool is_valid_subrange(integer_type offset, integer_type count = integer_type(-1)) const noexcept
  {
    return is_subrange( subrange_unchecked(offset, count) );
  }

  integer_range subrange(integer_type offset, integer_type count = integer_type(-1)) const noexcept
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
constexpr T ror(T x, int16_t n) noexcept
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
constexpr T rol(T x, int16_t n) noexcept
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

// todo: windows only, add fallback impl
template <typename T, std::enable_if_t<sizeof(T) <= 4, int> = 0>
FORCE_INLINE unsigned long ctz(const T value) noexcept
{
  unsigned long index = 0;
  return _BitScanForward(&index, static_cast<DWORD>(value)) ? index : sizeof(T) * 8;
}

template <typename T, std::enable_if_t<sizeof(T) == 8, int> = 0>
FORCE_INLINE unsigned long ctz(const T value) noexcept
{
  unsigned long index = 0;
  return _BitScanForward64(&index, static_cast<DWORD64>(value)) ? index : 64;
}

template <typename T, std::enable_if_t<sizeof(T) <= 4, int> = 0>
FORCE_INLINE unsigned long clz(const T value) noexcept
{
  unsigned long index = 0;
  return _BitScanReverse(&index, static_cast<DWORD>(value)) ? ((sizeof(T) * 8) - (index + 1)) : (sizeof(T) * 8);
}

template <typename T, std::enable_if_t<sizeof(T) == 8, int> = 0>
FORCE_INLINE unsigned long clz(const T value) noexcept
{
  unsigned long index = 0;
  return _BitScanReverse64(&index, static_cast<DWORD64>(value)) ? (63 - index) : 64;
}

template <int LSB, int Size, typename T>
FORCE_INLINE T read_bitfield(T& v) noexcept
{
  return (v >> LSB) & ((1 << Size) - 1);
} 

template <int LSB, typename T>
FORCE_INLINE T read_bitfield(T& v) noexcept
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
  {
    return std::make_pair(it, false);
  }

  return std::make_pair(vec.insert(it, item), true);
}

template <typename T>
std::pair<typename std::vector<T>::iterator, bool>
insert_sorted_nodupe(std::vector<T>& vec, typename std::vector<T>::iterator start, const T& item)
{
  auto it = std::lower_bound(start, vec.end(), item);
  if (it != vec.end() && *it == item)
  {
    return std::make_pair(it, false);
  }

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

