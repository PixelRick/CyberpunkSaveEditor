#pragma once
#include "stringpool.hpp"

namespace cp {

struct gname
{
  gname()
  {
    m_idx = gnames_pool().register_string("uninitialized");
  }

  gname(const char* s)
    : gname(std::string_view(s)) {}

  gname(std::string_view s)
  {
    m_idx = gnames_pool().register_string(s);
  }

  gname(const gname&) = default;
  gname& operator=(const gname&) = default;

  friend inline bool operator<(const gname& a, const gname& b) {
    return a.m_idx < b.m_idx;
  }

  friend inline bool operator==(const gname& a, const gname& b) {
    return a.m_idx == b.m_idx;
  }

  friend inline bool operator!=(const gname& a, const gname& b) {
    return !(a == b);
  }

  // Returned string_view has static storage duration
  std::string_view str() const
  {
    return gnames_pool().at(m_idx);
  }

  uint32_t idx() const { return m_idx; }

protected:
  static stringpool& gnames_pool()
  {
    static stringpool instance;
    return instance;
  }

  uint32_t m_idx;
};

} // namespace cp

namespace std {

template <>
struct hash<cp::gname>
{
  std::size_t operator()(const cp::gname& k) const
  {
    return hash<uint32_t>()(k.idx());
  }
};

} // namespace std

