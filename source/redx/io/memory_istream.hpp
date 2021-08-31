#pragma once
#include <fstream>
#include <filesystem>
#include <redx/common.hpp>

namespace redx {

// Input memory stream
struct memory_istream
  : streambase
{
  ~memory_istream() override = default;

  memory_istream(std::span<const char> span)
    : m_span(span), m_pos(0) {}

  memory_istream(const char* data, size_t size)
    : m_span(data, size), m_pos(0) {}

  bool is_reader() const override
  {
    return true;
  }

  pos_type tell() const override
  {
    return m_pos;
  }

  streambase& seek(pos_type pos) override
  {
    m_pos = pos;
    return *this;
  }

  streambase& seek(off_type off, seekdir dir) override
  {
    switch (dir)
    {
      case beg:
        m_pos = off;
        break;
      case cur:
        m_pos += off;
        break;
      case end:
        m_pos = m_span.size() + off;
        break;
    }
    return *this;
  }

  streambase& serialize_bytes(void* data, size_t size) override
  {
    if (m_pos >= 0 && m_pos + size <= m_span.size())
    {
      memcpy(data, m_span.data() + m_pos, size);
      m_pos += size;
    }
    else
    {
      set_error("imemstream: out-of-bounds read");
    }

    return *this;
  }

  template <typename T>
  const T* view()
  {
    const T* ret = nullptr;

    if (m_pos >= 0 && m_pos + sizeof(T) <= m_span.size())
    {
      ret = reinterpret_cast<T*>(m_span.data() + m_pos);
      m_pos += sizeof(T);
    }
    else
    {
      set_error("imemstream: out-of-bounds read");
    }

    return ret;
  }

  template <typename T>
  std::span<const T> view_array(size_t cnt)
  {
    std::span<const T> ret;

    const size_t size = cnt * sizeof(T);

    if (m_pos >= 0 && m_pos + size <= m_span.size())
    {
      ret = std::span<T>(m_span.data() + m_pos, cnt);
      m_pos += size;
    }
    else
    {
      set_error("imemstream: out-of-bounds read");
    }

    return ret;
  }

  const char* data() const
  {
    return m_span.data();
  }

  size_t size() const
  {
    return m_span.size();
  }

protected:

  std::span<const char> m_span;
  pos_type m_pos = 0;
};

} // namespace redx

