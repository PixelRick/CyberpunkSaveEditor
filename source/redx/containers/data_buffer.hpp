#pragma once
#include <type_traits>
#include <memory>

#include <redx/core.hpp>

namespace redx {

// buffer with dynamic alignment
// only uses default allocation for now.. (maybe move rt_allocator to common ?)
struct data_buffer
{
  data_buffer()
    : m_buf(nullptr, *this) {}

  data_buffer(const data_buffer&) = delete;
  data_buffer& operator=(const data_buffer&) = delete;

  data_buffer(data_buffer&&) = default;
  data_buffer& operator=(data_buffer&& other)
  {
    m_buf.swap(other.m_buf);
    std::swap(m_size, other.m_size);
    std::swap(m_alignment, other.m_alignment);
  }

  void* data()
  {
    return m_buf.get();
  }

  const void* data() const
  {
    return m_buf.get();
  }

  size_t size() const
  {
    return m_size;
  }

  size_t alignment() const
  {
    return m_alignment;
  }

  void reset(size_t size = 0, size_t alignment = 0)
  {
    m_buf.reset();
    m_alignment = alignment;
    m_size = 0;
    void* p = nullptr;
    if (size)
    {
      if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      {
        p = ::operator new (size, std::align_val_t{alignment});
      }
      else
      {
        p = ::operator new (size);
      }
      m_size = p ? size : 0;
      m_buf.reset(p);
    }
  }

private:

  // deleter
  void operator()(void* p) const
  {
    if (m_alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
    {
      ::operator delete (p, m_size, std::align_val_t{m_alignment});
    }
    else
    {
      ::operator delete (p, m_size);
    }
  };

  std::unique_ptr<void, data_buffer&> m_buf;
  size_t m_size = 0;
  size_t m_alignment = 0;
};

static_assert(std::is_move_constructible_v<data_buffer>);
static_assert(std::is_move_assignable_v<data_buffer>);

} // namespace redx

