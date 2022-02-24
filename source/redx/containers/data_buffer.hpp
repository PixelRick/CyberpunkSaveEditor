#pragma once
#include <type_traits>

#include <redx/core.hpp>

namespace redx {

// buffer with dynamic alignment
// only uses default allocation for now.. (maybe move rtts::object_allocator to common ?)
struct data_buffer
{
  data_buffer() noexcept = default;

  data_buffer(const data_buffer&) = delete;
  data_buffer& operator=(const data_buffer&) = delete;

  data_buffer(data_buffer&&) = default;
  data_buffer& operator=(data_buffer&& other) = default;

  ~data_buffer()
  {
    if (m_buf)
    {
      deallocate();
    }
  }

  void* data()
  {
    return m_buf;
  }

  const void* data() const
  {
    return m_buf;
  }

  size_t size() const
  {
    return m_size;
  }

  size_t alignment() const
  {
    return m_alignment;
  }

  void reset()
  {
    if (m_buf)
    {
      deallocate();
      m_buf = nullptr;
      m_size = 0;
    }
  }

  void reset(size_t size, size_t alignment = 0)
  {
    reset();
    m_alignment = alignment;
    if (size)
    {
      m_buf = aligned_storage_allocator::allocate(size, m_alignment);
      m_size = m_buf ? size : 0;
    }
  }

private:

  void deallocate() const
  {
    aligned_storage_allocator::deallocate(m_buf, m_size, m_alignment);
  };

  void* m_buf = nullptr;
  size_t m_size = 0;
  size_t m_alignment = 0;
};

static_assert(std::is_move_constructible_v<data_buffer>);
static_assert(std::is_move_assignable_v<data_buffer>);

} // namespace redx

