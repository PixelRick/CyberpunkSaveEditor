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

//--------------------------------------------------------
//  std lib helpers

template <typename EnumT>
constexpr std::underlying_type_t<EnumT> to_underlying(EnumT e) noexcept
{
  return static_cast<std::underlying_type_t<EnumT>>(e);
}

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
inline constexpr bool is_iterator_v = false;

template <typename T>
inline constexpr bool is_iterator_v<T, std::void_t<typename std::iterator_traits<T>::iterator_category>> = true;

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

//--------------------------------------------------------
//  casts helpers

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
inline constexpr bool is_unsafe_numeric_cast_v = is_unsafe_numeric_cast<To, From>::value;

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
  const T ret = static_cast<T>(x);
  ok = true;

  if constexpr (is_unsafe_numeric_cast_v<T, U>)
  {
    if (static_cast<U>(ret) != x)
    {
      // print only in debug since caller is in charge of handling the error
      SPDLOG_DEBUG("numeric_cast error {} -> {}", x, ret);
      ok = false;
    }
  }

  return ret;
}

//--------------------------------------------------------
//  alignment helpers

// alignment is assumed to be a power of two
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
FORCE_INLINE constexpr [[nodiscard]] T align_up(T address, size_t alignment)
{
  return (address + alignment - 1) & ~(alignment - 1);
}

// alignment is assumed to be a power of two
FORCE_INLINE constexpr [[nodiscard]] void* align_up(void* address, size_t alignment)
{
  return (void*)(align_up(reinterpret_cast<uintptr_t>(address), alignment));
}

// alignment is assumed to be a power of two
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
FORCE_INLINE constexpr [[nodiscard]] T align_down(T address, size_t alignment)
{
  return (address) & ~(alignment - 1);
}

// alignment is assumed to be a power of two
FORCE_INLINE constexpr [[nodiscard]] void* align_down(void* address, size_t alignment)
{
  return (void*)(align_down(reinterpret_cast<uintptr_t>(address), alignment));
}

template <size_t alignment>
FORCE_INLINE constexpr [[nodiscard]] void* align_up(void* address)
{
  static_assert((alignment & (alignment - 1)) == 0, "alignment must be a power of two");
  return (void*)(((uintptr_t)address + alignment - 1) & ~(alignment - 1));
}

//--------------------------------------------------------
//  string helpers

inline bool starts_with(const std::string& str, const std::string& with)
{
  return with.length() <= str.length()
    && std::equal(with.begin(), with.end(), str.begin());
}

//--------------------------------------------------------
//  ranges

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

//--------------------------------------------------------
//  bit-ops

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
  return _BitScanForward(&index, static_cast<uint32_t>(value)) ? index : sizeof(T) * 8;
}

#ifdef _M_X64
template <typename T, std::enable_if_t<sizeof(T) == 8, int> = 0>
FORCE_INLINE unsigned long ctz(const T value) noexcept
{
  unsigned long index = 0;
  return _BitScanForward64(&index, static_cast<uint64_t>(value)) ? index : 64;
}
#endif

template <typename T, std::enable_if_t<sizeof(T) <= 4, int> = 0>
FORCE_INLINE unsigned long clz(const T value) noexcept
{
  unsigned long index = 0;
  return _BitScanReverse(&index, static_cast<uint32_t>(value)) ? ((sizeof(T) * 8) - (index + 1)) : (sizeof(T) * 8);
}

#ifdef _M_X64
template <typename T, std::enable_if_t<sizeof(T) == 8, int> = 0>
FORCE_INLINE unsigned long clz(const T value) noexcept
{
  unsigned long index = 0;
  return _BitScanReverse64(&index, static_cast<uint64_t>(value)) ? (63 - index) : 64;
}
#endif

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

//--------------------------------------------------------
//  misc

struct fourcc
{
  constexpr fourcc() = default;

  constexpr explicit fourcc(uint32_t val)
    : m_val(val) {}

  fourcc& operator=(uint32_t val)
  {
    m_val = val;
  }

  constexpr bool operator==(const fourcc& rhs) const noexcept
  {
    return (m_val == rhs.m_val);
  }

  constexpr bool operator!=(const fourcc& rhs) const noexcept
  {
    return !operator==(rhs);
  }

  std::string str() const
  {
    const uint32_t reversed = byteswap(m_val);
    const char* pc = reinterpret_cast<const char*>(&reversed);
    return std::string(pc, pc + 4);
  }

  constexpr operator uint32_t() const noexcept
  {
    return m_val;
  }

private:

  uint32_t m_val = 'NONE';
};

static_assert(std::is_trivially_copyable_v<fourcc>);

//--------------------------------------------------------
//  container helpers

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

//--------------------------------------------------------
//  fast binary search

inline constexpr uintptr_t sse2_strstr_npos = uintptr_t(-1);

// values > 0xFF are considered wildcards
// mask must not begin nor end with a wildcard value
uintptr_t sse2_strstr_masked(uintptr_t haystack, size_t haystack_size, const wchar_t* masked_needle, size_t needle_size);
uintptr_t sse2_strstr(uintptr_t haystack, size_t haystack_sizem, const char* needle, size_t needle_size);

// values > 0xFF are considered wildcards
// mask must not begin nor end with a wildcard value
inline uintptr_t sse2_strstr_masked(const void* haystack, size_t haystack_size, const wchar_t* masked_needle, size_t needle_size)
{
  return sse2_strstr_masked((uintptr_t)haystack, haystack_size, masked_needle, needle_size);
}

inline uintptr_t sse2_strstr(const void* haystack, size_t haystack_size, const char* needle, size_t needle_size)
{
  return sse2_strstr((uintptr_t)haystack, haystack_size, needle, needle_size);
}

} // namespace redx

