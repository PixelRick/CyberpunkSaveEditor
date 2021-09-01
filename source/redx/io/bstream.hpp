#pragma once
#include <inttypes.h>
#include <optional>
#include <ios>

#include <redx/core/platform.hpp>
#include <redx/core/utils.hpp>

// stream classes are either for input or output but not both.
// this makes the expected usage explicit and spares us from virtual inheritance
// if both accesses are necessary the user could instantiate two different
// ones with a shared underlying bstreambuf..

namespace redx {

using bstreampos = uint64_t;

template <class T, class = void>
struct is_trivially_serializable
{
  static constexpr bool value = std::is_trivially_copyable_v<T>;
};

template <typename T>
inline constexpr bool is_trivially_serializable_v = is_trivially_serializable<T>::value;

// base class for ibstream and obstream.
struct bstream
{
  using traits_type = std::streambuf::traits_type;

  bstream(std::streambuf& sbuf) noexcept
    : m_sbuf(sbuf) {}

  virtual ~bstream() noexcept = default;

  bstream(const bstream&) = delete;
  bstream& operator=(const bstream&) = delete;

  explicit FORCE_INLINE operator bool() const noexcept
  {
    return !fail();
  }

  FORCE_INLINE bool operator!() const noexcept
  {
    return !bool(*this);
  }

  FORCE_INLINE std::streambuf& sbuf() const noexcept
  {
    return m_sbuf;
  }

#pragma region state

  [[nodiscard]] FORCE_INLINE bool fail() const noexcept
  {
    return m_fail;
  }

  FORCE_INLINE void set_fail() noexcept
  {
    m_fail = true;
  }

  FORCE_INLINE void clear_fail() noexcept
  {
    m_fail = false;
    clear_fail_msg();
  }

  // if there is a previous msg, does nothing.
  // this does set the failbit.
  inline void set_fail_with_msg(std::string msg) noexcept
  {
    set_fail();

    // keep first non empty error
    if (!m_fail_msg.has_value() && !msg.empty())
    {
      m_fail_msg = msg;
    }
  }

  // this doesn't clear the fail state
  FORCE_INLINE void clear_fail_msg() noexcept
  {
    m_fail_msg.reset();
  }

  inline std::string fail_msg() const
  {
    return m_fail_msg.value_or(fail() ? "unknown error" : "");
  }

#pragma endregion

protected:

  FORCE_INLINE void set_sbuf(std::streambuf& sb) noexcept
  {
    m_sbuf = sb;
    clear_fail();
  }

private:

  std::reference_wrapper<std::streambuf> m_sbuf;
  std::optional<std::string> m_fail_msg;
  bool m_fail = false;
};

// base class for input byte streams.
struct ibstream
  : bstream
{
  using bstream::bstream;

  ~ibstream() noexcept override = default;

#pragma region operators

  // little-endian only

  // read trivial types
  template <typename T, std::enable_if_t< 
    is_trivially_serializable_v<T> && 
    !std::is_arithmetic_v<T> && 
    !std::is_const_v<T>, bool> = true> 
  FORCE_INLINE ibstream& operator>>(T& val) 
  { 
    return read_bytes((char*)&val, sizeof(T)); 
  }

  // read arithmetic types of size >1
  template <typename T, std::enable_if_t<
    std::is_arithmetic_v<T> &&
    (sizeof(T) > 1) &&
    !std::is_const_v<T>, bool> = true>
  FORCE_INLINE ibstream& operator>>(T& val)
  {
    val = static_cast<T>(read_byte());
    return read_bytes((char*)&val, sizeof(T)); 
  }

  // read arithmetic types of size 1
  template <typename T, std::enable_if_t<
    std::is_arithmetic_v<T> &&
    (sizeof(T) == 1) &&
    !std::is_const_v<T>, bool> = true>
  FORCE_INLINE ibstream& operator>>(T& val)
  {
    val = static_cast<T>(read_byte());
    return *this;
  }

  // read bool
  FORCE_INLINE ibstream& operator>>(bool& val)
  {
    val = read_byte() != 0;
    return *this;
  }

#pragma endregion

#pragma region streambuf ops

  inline bstreampos tellg() const
  {
    auto& sb = sbuf();

    if (!fail())
    {
      return sb.pubseekoff(0, std::ios::cur, std::ios::in);
    }
    
    return bstreampos(-1);
  }

  // doesn't clear fail, it's safer this way..
  inline bstream& seekg(bstreampos pos)
  {
    auto& sb = sbuf();

    if (!sb.pubseekpos(std::streampos(pos), std::ios::in))
    {
      set_fail_with_msg(
        fmt::format("failed seekpos({})", pos));
    }

    return *this;
  }

  // doesn't clear fail, it's safer this way..
  inline bstream& seekg(std::streamoff off, std::ios::seekdir dir)
  {
    auto& sb = sbuf();

    if (!sb.pubseekoff(off, dir, std::ios::in))
    {
      set_fail_with_msg(
        fmt::format("failed seekpoff({}, {})", off, dir));
    }

    return *this;
  }

  std::streamsize size()
  {
    auto& sb = sbuf();
    if (!fail())
    {
      auto saved = sb.pubseekoff(0, std::ios::cur);;
      auto ret = sb.pubseekoff(0, std::ios::end);
      sb.pubseekpos(saved);
      return std::streamsize(ret);
    }

    return std::streamsize(-1);
  }

  char read_byte()
  {
    auto& sb = sbuf();

    if (!fail())
    {
      auto onec = sb.sbumpc();
      if (!traits_type::eq_int_type(onec, traits_type::eof()))
      {
        return static_cast<char>(onec);
      }
      set_fail();
    }

    return 0;
  }
  
  ibstream& read_bytes(void* dst, size_t count)
  {
    auto& sb = sbuf();
    const std::streamsize ssize = reliable_numeric_cast<std::streamsize>(count);
    // skip checking for overflow.. 

    if (!fail())
    {
      const auto rsize = sb.sgetn((char*)dst, ssize);
      if (rsize != ssize)
      {
        set_fail();
      }
    }

    return *this;
  }

#pragma endregion

#pragma region read ops

  FORCE_INLINE ibstream& read_bytes(const std::span<char>& span)
  {
    return read_bytes(span.data(), span.size());
  }

  // for arithmetic types, so that one can use T v = read<T>(), that makes size explicit
  template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && (sizeof(T) > 1), bool> = true>
  FORCE_INLINE T read()
  {
    T ret = {};
    *this >> ret;
    return ret;
  }

  // for arithmetic types, so that one can use T v = read<T>(), that makes size explicit
  template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && (sizeof(T) == 1), bool> = true>
  FORCE_INLINE T read()
  {
    return static_cast<T>(read_byte());
  }

  // depending on T, either reads the bytes span or reads element by element
  template <typename T, std::enable_if_t<!std::is_const_v<T>, bool> = true>
  inline ibstream& read_array(T* arr, size_t count)
  {
    if constexpr (is_trivially_serializable_v<T>)
    {
      read_bytes(reinterpret_cast<char*>(arr), count * sizeof(T));
    }
    else
    {
      T* const end = arr + count;
      for (T* it = arr; it != end; ++it)
      {
        *this >> *it;
      }
    }

    if constexpr (std::is_default_constructible_v<T> && std::is_copy_assignable_v<T>)
    {
      if (fail())
      {
        for (size_t i = 0; i < count; ++i)
        {
          arr[i] = {};
        }
      }
    }

    return *this;
  }

  // depending on T, either reads the bytes span or reads element by element
  template <typename T, std::enable_if_t<!std::is_const_v<T>, bool> = true>
  FORCE_INLINE ibstream& read_array(const std::span<T>& span)
  {
    return read_array<T>(span.data(), span.size());
  }

  // depending on T, either reads the bytes span or reads element by element
  template <typename T, std::enable_if_t<!std::is_const_v<T> && !std::is_same_v<T, bool>, bool> = true>
  FORCE_INLINE ibstream& read_array(std::vector<T>& span)
  {
    return read_array<T>(span.data(), span.size());
  }

  template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_const_v<T>, bool> = true>
  FORCE_INLINE ibstream& read_int_packed(T& out_value)
  {
    out_value = static_cast<T>(read_int64_packed());
    return *this;
  }

  template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_const_v<T>, bool> = true>
  FORCE_INLINE T read_int_packed()
  {
    return static_cast<T>(read_int64_packed());
  }

  // by default, vectors are serialized with a length prefix
  template <typename T, std::enable_if_t<!std::is_const_v<T>, bool> = true>
  ibstream& read_vec_lpfxd(std::vector<T>& vec)
  {
    if (!fail())
    {
      uint32_t cnt = 0;
      *this >> cnt;

      if (cnt > 0x1000000)
      {
        set_fail_with_msg("read vector size is too big");
      }
      else if (cnt > 0)
      {
        vec.resize(cnt);
        read_array(std::span<T>(vec));
      }
    }

    if (fail())
    {
      vec.clear();
    }

    return *this;
  }

  // std::vector<bool> uses a bitfield impl
  // so let's use 1 byte per bool intermediate vec
  ibstream& read_vec_lpfxd(std::vector<bool>& vec)
  {
    vec.clear();

    std::vector<uint8_t> u8vec;
    read_vec_lpfxd(u8vec);

    if (!fail())
    {
      vec.reserve(u8vec.size());
      std::copy(u8vec.begin(), u8vec.end(), std::back_inserter(vec));
    }

    return *this;
  }

  // TODO: overload for std::u16string
  ibstream& read_str_lpfxd(std::string& s);
  int64_t read_int64_packed();

#pragma endregion

};

// to wrap any std::streambuf
struct generic_ibstream
  : ibstream
{
  generic_ibstream() noexcept
    : ibstream(m_dummy) {}

  generic_ibstream(std::streambuf& sb) noexcept
    : ibstream(sb) {}

  FORCE_INLINE void set_sbuf(std::streambuf& sb)
  {
    ibstream::set_sbuf(sb);
  }

private:

  struct dummy_buf
    : std::streambuf {};

  dummy_buf m_dummy;
};

// base class for output byte streams.
struct obstream
  : bstream
{
  using bstream::bstream;

  ~obstream() noexcept override = default;

#pragma region operators

  // little-endian only

  // write trivial types
  template <typename T, std::enable_if_t<
    is_trivially_serializable_v<T> &&
    !std::is_arithmetic_v<T>, bool> = true>
  FORCE_INLINE obstream& operator<<(const T& val)
  {
    return write_bytes((const char*)&val, sizeof(T));
  }

  // write arithmetic types of size >1
  template <typename T, std::enable_if_t<
    std::is_arithmetic_v<T> &&
    (sizeof(T) > 1), bool> = true>
  FORCE_INLINE obstream& operator<<(const T& val)
  {
    return write_bytes((const char*)&val, sizeof(T));
  }

  // write arithmetic types of size 1
  template <typename T, std::enable_if_t<
    std::is_arithmetic_v<T> &&
    (sizeof(T) == 1), bool> = true>
  FORCE_INLINE obstream& operator<<(const T& val)
  {
    return write_byte(static_cast<char>(val));
  }

  // write bool
  FORCE_INLINE obstream& operator<<(const bool& val)
  {
    return write_byte(val ? 1 : 0);
  }

#pragma endregion

#pragma region streambuf ops

  inline bstreampos tellp() const
  {
    auto& sb = sbuf();

    if (!fail())
    {
      return sb.pubseekoff(0, std::ios::cur, std::ios::out);
    }
    
    return bstreampos(-1);
  }

  // doesn't clear fail, it's safer this way..
  inline bstream& seekp(bstreampos pos)
  {
    auto& sb = sbuf();

    if (!sb.pubseekpos(std::streampos(pos), std::ios::out))
    {
      set_fail_with_msg(
        fmt::format("failed seekpos({})", pos));
    }

    return *this;
  }

  // doesn't clear fail, it's safer this way..
  inline bstream& seekp(std::streamoff off, std::ios::seekdir dir)
  {
    auto& sb = sbuf();

    if (!sb.pubseekoff(off, dir, std::ios::out))
    {
      set_fail_with_msg(
        fmt::format("failed seekpoff({}, {})", off, dir));
    }

    return *this;
  }

  obstream& write_byte(char b)
  {
    auto& sb = sbuf();

    if (!fail())
    {
      if (traits_type::eq_int_type(sb.sputc(b), traits_type::eof()))
      {
        set_fail();
      }
    }

    return *this;
  }

  obstream& write_bytes(const void* src, size_t count)
  {
    auto& sb = sbuf();
    const std::streamsize ssize = reliable_numeric_cast<std::streamsize>(count);
    // skip checking for overflow.. 

    if (!fail())
    {
      if (sb.sputn((const char*)src, ssize) != ssize)
      {
        set_fail();
      }
    }

    return *this;
  }

  obstream& flush()
  {
    auto& sb = sbuf();

    if (!fail() && traits_type::eq_int_type(sb.pubsync(), traits_type::eof()))
    {
      set_fail();
    }

    return *this;
  }

#pragma endregion

#pragma region write ops

  // for arithmetic types, so that one can use write<T>(v), that makes size explicit
  template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && (sizeof(T) > 1), bool> = true>
  FORCE_INLINE obstream& write(const T& value)
  {
    return *this << value;
  }

  // for arithmetic types, so that one can use write<T>(v), that makes size explicit
  template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && (sizeof(T) == 1), bool> = true>
  FORCE_INLINE obstream& write(const T& value)
  {
    return write_byte(static_cast<char>(value));
  }

  FORCE_INLINE obstream& write_bytes(const std::span<char>& span)
  {
    return write_bytes(span.data(), span.size());
  }

  FORCE_INLINE obstream& write_bytes(const std::span<const char>& span)
  {
    return write_bytes(span.data(), span.size());
  }

  // depending on T, either writes the bytes span or writes element by element
  template <typename T>
  inline obstream& write_array(const T* arr, size_t count)
  {
    if constexpr (is_trivially_serializable_v<T>)
    {
      write_bytes(reinterpret_cast<const char*>(arr), count * sizeof(T));
    }
    else
    {
      const T* const end = arr + count;
      for (const T* it = arr; it != end; ++it)
      {
        *this << *it;
      }
    }

    return *this;
  }

  // depending on T, either writes the bytes span or writes element by element
  template <typename T>
  FORCE_INLINE obstream& write_array(const std::span<const T>& span)
  {
    return write_array<T>(span.data(), span.size());
  }

  // depending on T, either writes the bytes span or writes element by element
  template <typename T, std::enable_if_t<!std::is_same_v<T, bool>, bool> = true>
  FORCE_INLINE obstream& write_array(const std::vector<T>& span)
  {
    return write_array<T>(span.data(), span.size());
  }

  template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
  FORCE_INLINE obstream& write_int_packed(const T& value)
  {
    write_int64_packed(static_cast<int64_t>(value));
    return *this;
  }

  // by default, vectors are serialized with a length prefix
  template <typename T>
  obstream& write_vec_lpfxd(const std::vector<T>& vec)
  {
    if (!fail())
    {
      size_t cnt = vec.size();
      if (cnt > 0x1000000)
      {
        set_fail_with_msg("read vector size is too big");
      }
      else
      {
        const uint32_t u32cnt = static_cast<uint32_t>(cnt);
        write<uint32_t>(u32cnt);
        write_array(std::span<const T>(vec));
      }
    }

    return *this;
  }

  // std::vector<bool> uses a bitfield impl
  // so let's use 1 byte per bool intermediate vec
  obstream& write_vec_lpfxd(const std::vector<bool>& vec)
  {
    if (!fail())
    {
      std::vector<uint8_t> u8vec;
      u8vec.resize(vec.size());
      std::copy(vec.begin(), vec.end(), u8vec.begin());
      write_vec_lpfxd(u8vec);
    }

    return *this;
  }

  obstream& write_str_lpfxd(const std::string& s);
  obstream& write_int64_packed(int64_t v);

#pragma endregion

};

// to wrap any bstreambuf
struct generic_obstream
  : obstream
{
  generic_obstream() noexcept
    : obstream(m_dummy) {}

  generic_obstream(std::streambuf& sb) noexcept
    : obstream(sb) {}

  FORCE_INLINE void set_sbuf(std::streambuf& sb)
  {
    obstream::set_sbuf(sb);
  }

private:

  struct dummy_buf
    : std::streambuf {};

  dummy_buf m_dummy;
};

} // namespace redx

#define STREAM_LOG_AND_SET_ERROR(st, sfmt, ...) \
  do { \
    auto err = fmt::format("[{} line {}]: " sfmt, SPDLOG_FUNCTION, __LINE__, __VA_ARGS__); \
    SPDLOG_ERROR(err); \
    (st).set_fail_with_msg(err); \
  } while(0)

