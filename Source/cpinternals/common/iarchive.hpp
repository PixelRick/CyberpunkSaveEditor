#pragma once
#include <inttypes.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>

#include "iserializable.hpp"

namespace cp {

namespace armanip {

enum arflags_t : uint32_t
{
  none = 0,
  cnamehash = 1 << 0,
  cnamestr = 1 << 1,
  all = 0xFFFFFFFF
};

} // namespace armanip

struct iarchive
{
  using pos_type = std::streamoff;
  using off_type = std::streampos;
  using flags_type = armanip::arflags_t;

  virtual ~iarchive() = default;

  virtual bool is_reader() const = 0;

  virtual pos_type tell() const = 0;
  virtual iarchive& seek(pos_type pos) = 0;
  virtual iarchive& seek(off_type off, std::istream::seekdir dir) = 0;

  virtual iarchive& serialize(void* data, size_t len) = 0;

  void set_error(std::string error)
  {
    m_error = error;
  }

  void clear_error()
  {
    m_error.clear();
  }

  bool has_error() const
  {
    return !m_error.empty();
  }

  void clrflags()
  {
    m_flags = flags_type::none;
  }

  flags_type flags() const
  {
    return m_flags;
  }

  iarchive& operator<<(flags_type flags)
  {
    // TODO: needs custom logic helpers for incompatible manipulators
    //       mask table ?

    using utype = std::underlying_type<flags_type>::type;
    utype in_flags = static_cast<utype>(flags);
    utype new_flags = static_cast<utype>(m_flags);

    // cname related
    new_flags &= ~(flags_type::cnamehash | flags_type::cnamestr);
    if (in_flags & flags_type::cnamehash)
    {
      new_flags |= flags_type::cnamehash;
    }
    // No else, cnamestr is the default even if not set
    in_flags &= ~(flags_type::cnamehash | flags_type::cnamestr);

    new_flags |= in_flags; // merge the rest of the input flags

    m_flags = static_cast<flags_type>(new_flags);
    return *this;
  }

  std::string error() const
  {
    return m_error;
  }

  iarchive& byte_order_serialize(void* value, uint64_t length)
  {
    // To be implemented if necessary
    serialize(value, length);
    return *this;
  }

  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_const_v<T> && !std::is_same_v<T, bool>>>
  iarchive& operator<<(T& val)
  {
    byte_order_serialize(&val, sizeof(T));
    return *this;
  }

  iarchive& operator<<(bool& val)
  {
    uint8_t u8 = val ? 1 : 0;
    serialize(&u8, 1);
    val = !!u8;
    return *this;
  }

  template <typename T, typename = std::enable_if_t<!std::is_same_v<T, bool>>>
  iarchive& operator<<(std::vector<T>& vec)
  {
    uint32_t cnt = static_cast<uint32_t>(vec.size());
    *this << cnt;
    if (cnt > 0x40000)
      throw std::range_error("serialized vector size is too big");
    vec.resize(cnt);

    for (T& it : vec)
    {
      *this << it;
    }

    return *this;
  }

  iarchive& operator<<(std::vector<bool>& vec)
  {
    uint32_t cnt = static_cast<uint32_t>(vec.size());
    *this << cnt;
    vec.resize(cnt);

    for (auto it : vec)
    {
      bool x = it;
      *this << x;
      it = x;
    }

    return *this;
  }

  iarchive& operator<<(std::string& s)
  {
    return serialize_str_lpfxd(s);
  }

  template <typename T>
  iarchive& serialize_pod_raw(T& value)
  {
    serialize(&value, sizeof(T));
    return *this;
  }

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  iarchive& serialize_int_packed(T& v)
  {
    if (is_reader())
    {
      v = static_cast<T>(read_int_packed());
    }
    else
    {
      write_int_packed(static_cast<int64_t>(v));
    }
    return *this;
  }

  // TODO: overload for std::u16string
  iarchive& serialize_str_lpfxd(std::string& s);

protected:
  int64_t read_int_packed();
  void write_int_packed(int64_t v);

  std::string m_error;
  flags_type m_flags = flags_type::none;
};


} // namespace cp

