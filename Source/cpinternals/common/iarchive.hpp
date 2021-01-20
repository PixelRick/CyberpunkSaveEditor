#pragma once
#include <inttypes.h>
#include <iostream>
#include <array>
#include <cmath>

#include "iserializable.hpp"

namespace cp {

struct iarchive
{
  using pos_type = std::streamoff;
  using off_type = std::streampos;

  virtual ~iarchive() = default;

  virtual bool is_reader() = 0;

  virtual pos_type tell() = 0;
  virtual iarchive& seek(pos_type pos) = 0;
  virtual iarchive& seek(off_type off, std::istream::off_type dir) = 0;

  virtual iarchive& serialize(void* data, size_t len) = 0;

  iarchive& byte_order_serialize(void* value, uint64_t length)
  {
    // To be implemented if necessary
    serialize(value, length);
    return *this;
  }

  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_const_v<T>>>
  iarchive& operator<<(T& val)
  {
    byte_order_serialize(&val, sizeof(T));
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
};


} // namespace cp

