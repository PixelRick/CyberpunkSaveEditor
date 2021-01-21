#pragma once
#include "cpinternals/common.hpp"

namespace cp::tdb {

template <typename T, int ReserveCnt = 0x100>
struct value_pool
{
  using value_type = T;

  value_pool()
  {
    m_values.reserve(ReserveCnt);
  }

  size_t size() const
  {
    return m_values.size();
  }

  const value_type& at(size_t idx) const
  {
    return m_values[idx];
  }

  // Returns the index of the value in pool.
  const size_t insert(const value_type& val)
  {
    uint32_t idx = 0;
    auto it = lower_bound_idx(val);
    if (it != m_sorted_indices.end())
    {
      idx = (uint32_t)*it;
      if (val == at(idx))
        return idx;
    }

    idx = (uint32_t)m_values.size();
    m_values.emplace_back(val);
    m_sorted_indices.insert(it, idx);

    return idx;
  }

  // This is used for serialiation
  const void push_back(const value_type& val)
  {
    const uint32_t idx = (uint32_t)m_values.size();
    m_values.emplace_back(val);

    auto it = lower_bound_idx(val);
    m_sorted_indices.insert(it, idx);
  }

  bool has_value(const value_type& val) const
  {
    auto it = lower_bound_idx(val);
    return it != m_sorted_indices.end() && val == at(*it);
  }

  const std::vector<value_type>& values() const
  {
    return m_values;
  }

protected:
  std::vector<value_type> m_values;

  // Accelerated search

  std::vector<size_t> m_sorted_indices;

  struct search_value_t
  {
    const std::vector<value_type>& values;
    const value_type& ref;

    bool operator>(const size_t& idx) const
    {
      const bool ret = ref > values[idx];
      return ret;
    }

    friend bool operator<(const size_t& idx, const search_value_t& sv)
    {
      return sv.operator>(idx);
    }
  };

  std::vector<size_t>::const_iterator lower_bound_idx(std::string_view s) const
  {
    // TODO (in 2022): C++20 has a variant with Pred.
    return std::lower_bound(
      m_sorted_indices.begin(),
      m_sorted_indices.end(),
      search_value_t{this, s}
    );
  }
};

} // namespace cp::tdb