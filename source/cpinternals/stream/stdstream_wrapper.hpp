#pragma once
#include <iostream>
#include <type_traits>
#include <cpinternals/common.hpp>

namespace cp {


// cp streams can't do both in and out
template <typename StreamType>
struct stdstream_wrapper
  : public streambase
{
  using underlying_type = StreamType;

  constexpr static bool is_istream = std::is_base_of_v<std::istream, StreamType>;
  constexpr static bool is_ostream = std::is_base_of_v<std::ostream, StreamType>;

  static_assert(is_istream || is_ostream, "stdstream_wrapper<T>: T must inherit from std::istream or std::ostream");
  static_assert(!(is_istream && is_ostream), "stdstream_wrapper<T>: T cannot inherit from both std::istream and std::ostream");

  stdstream_wrapper(underlying_type& ref)
    : m_ref(ref)
  {
  }

  ~stdstream_wrapper() override = default;

  bool is_reader() const override
  {
    return is_istream;
  }

  pos_type tell() const override
  {
    if constexpr (is_istream)
    {
      return m_ref.tellg();
    }
    else
    {
      return m_ref.tellp();
    }
  }

  streambase& seek(pos_type pos) override
  {
    if constexpr (is_istream)
    {
      m_ref.seekg(pos);
    }
    else
    {
      m_ref.seekp(pos);
    }
    return *this;
  }

  streambase& seek(off_type off, std::istream::seekdir dir) override
  {
    if constexpr (is_istream)
    {
      m_ref.seekg(off, dir);
    }
    else
    {
      m_ref.seekp(off, dir);
    }
    return *this;
  }

  streambase& serialize_bytes(void* data, size_t len) override
  {
    if constexpr (is_istream)
    {
      m_ref.read(reinterpret_cast<char*>(data), len);
    }
    else
    {
      m_ref.write(reinterpret_cast<char*>(data), len);
    }
    return *this;
  }

protected:

  underlying_type& m_ref;
};

} // namespace cp

