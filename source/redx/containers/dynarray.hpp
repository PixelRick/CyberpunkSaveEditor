#pragma once
#include <type_traits>
#include <array>

#include <redx/core.hpp>

namespace redx {

// mix between std::array and std::vector
// dynamic size but local storage
template <typename T, size_t Size>
struct dynarray
{
  using arr_type = std::array<T, Size>;

  using value_type              = typename arr_type::value_type;
  using size_type               = typename arr_type::size_type;
  using difference_type         = typename arr_type::difference_type;
  using reference               = typename arr_type::reference;
  using const_reference         = typename arr_type::const_reference;
  using pointer                 = typename arr_type::pointer;
  using const_pointer           = typename arr_type::const_pointer;
  using iterator                = typename arr_type::iterator;
  using const_iterator          = typename arr_type::const_iterator;
  using reverse_iterator        = typename arr_type::reverse_iterator;
  using const_reverse_iterator  = typename arr_type::const_reverse_iterator;

  constexpr reference at(size_type pos) noexcept
  {
    if (m_size <= pos)
    {
      SPDLOG_CRITICAL("index out of range");
      DEBUG_BREAK();
    }

    return m_arr.at(pos);
  }

  constexpr const_reference at(size_type pos) const noexcept
  {
    return const_cast<sarray*>(this)->at(pos);
  }

  constexpr reference operator[](const size_type pos) noexcept
  {
    return at(pos);
  }
  
  constexpr const_reference operator[](const size_type pos) const noexcept
  {
    return at(pos);
  }

  constexpr reference front()
  {
    return m_arr.front();
  }

  constexpr const_reference front() const
  {
    return m_arr.front();
  }

  constexpr reference back()
  {
    return m_arr.at(m_size);
  }

  constexpr const_reference back() const
  {
    return m_arr.at(m_size);
  }

  constexpr T* data() noexcept
  {
    return m_arr.data();
  }

  constexpr const T* data() const noexcept
  {
    return m_arr.data();
  }

  constexpr iterator begin() noexcept
  {
    return m_arr.begin();
  }

  constexpr const_iterator begin() const noexcept
  {
    return m_arr.begin();
  }

  constexpr iterator end() noexcept
  {
    return m_arr.begin() + m_size;
  }

  constexpr const_iterator end() const noexcept
  {
    return m_arr.begin() + m_size;
  }

  constexpr bool empty() const noexcept
  {
    return m_size == 0;
  }

  constexpr size_type size() const noexcept
  {
    return m_size;
  }

  constexpr size_type max_size() const noexcept
  {
    return Size;
  }

  // only resize atm.. I don't know yet what ops will be necessary

  FORCE_INLINE bool resize(size_t new_size)
  {
    if (new_size >= Size)
    {
      return false;
    }

    if (new_size < m_size)
    {
      for (size_t i = new_size; i < m_size; ++i)
      {
        m_arr[i] = T();
      }
    }

    m_size = new_size;
    return true;
  }

protected: // not private for reflection

  size_t m_size = 0;
  std::array<T, Size> m_arr;
};

} // namespace redx

