#pragma once
#include "stringpool.hpp"
#include <nlohmann/json.hpp>

namespace cp {

// One global name type to rule them all.
struct gname
{
  constexpr gname() noexcept
    : m_idx(0)
  {
  }

  explicit gname(const char* s)
    : gname(std::string_view(s)) {}

  explicit gname(std::string_view s)
  {
    m_idx = gnames_pool().register_string(s);
  }

  explicit gname(const std::string& s)
    : gname(std::string_view(s))
  {
  }

  gname(const gname&) = default;
  gname& operator=(const gname&) = default;

  operator bool()
  {
    return !!m_idx;
  }

  friend inline bool operator<(const gname& a, const gname& b) {
    return a.strv() < b.strv();
  }

  friend inline bool operator==(const gname& a, const gname& b) {
    return a.m_idx == b.m_idx;
  }

  friend inline bool operator!=(const gname& a, const gname& b) {
    return !(a == b);
  }

  // Returned string_view has static storage duration
  std::string_view strv() const
  {
    return gnames_pool().at(m_idx);
  }

  const char* c_str() const
  {
    // string_pool guarantees a null terminator after the view.
    return strv().data();
  }

  operator std::string() const
  {
    return std::string(strv());
  }

  std::string string() const
  {
    return std::string(strv());
  }

  uint32_t idx() const { return m_idx; }

protected:
  friend struct literal_gname;

  static stringpool& gnames_pool()
  {
    static stringpool instance;
    static bool once = [](){
      instance.register_literal("<gname:uninitialized>");
      return true;
    }();
    return instance;
  }

  uint32_t m_idx;
};


inline void to_json(nlohmann::json& j, const gname& csn)
{
  j = csn.strv();
}

inline void from_json(const nlohmann::json& j, gname& csn)
{
  csn = gname(j.get<std::string>());
}

// TODO: rework this with C++20 (single static_gname per char* value)
struct literal_gname
{
  constexpr explicit literal_gname(const char* const s)
    : str(s)
  {}

  operator const gname&()
  {
    if (!gn)
      gn.m_idx = gname::gnames_pool().register_literal(str);
    return gn;
  }

protected:
  const char* const str;
  gname gn;
};

constexpr literal_gname operator""_gn(const char* str, std::size_t len)
{
  return literal_gname{str};
}

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

