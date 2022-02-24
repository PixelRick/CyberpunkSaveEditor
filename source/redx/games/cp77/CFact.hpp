#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <spdlog/spdlog.h>
#include "redx/core.hpp"

namespace redx {

/////////////////////////////////////////
// CFact
/////////////////////////////////////////

struct CFact
{
  CFact() = default;
  CFact(const CFact&) = default;

  CFact(uint32_t hash, uint32_t value)
    : m_hash(hash), m_value(value)
  {
  }

  CFact(std::string_view name, uint32_t value, bool add_to_resolver = true);

  CFact(const char* name, uint32_t value, bool add_to_resolver = true)
    : CFact(std::string_view(name), value, add_to_resolver)
  {
  }

  CFact(gname name, uint32_t value, bool add_to_resolver = true)
    : CFact(name.strv(), value, add_to_resolver)
  {
  }

  friend inline bool operator==(const CFact& a, const CFact& b)
  {
    return a.m_hash == b.m_hash && a.m_value == b.m_value;
  }

  gname name() const;

  uint32_t hash() const
  {
    return m_hash;
  }

  void hash(uint32_t hash)
  {
    m_hash = hash;
  }

  void name(gname name);

  uint32_t value() const
  {
    return m_value;
  }

  void value(uint32_t value)
  {
    m_value = value;
  }

protected:
  uint32_t m_hash = 0;
  uint32_t m_value = 0;
};


inline bool operator!=(const CFact& a, const CFact& b)
{
  return !(a == b);
}

/////////////////////////////////////////
// resolver
/////////////////////////////////////////

struct CFact_resolver
{
  static CFact_resolver& get()
  {
    static CFact_resolver s = {};
    return s;
  }

  CFact_resolver(const CFact_resolver&) = delete;
  CFact_resolver& operator=(const CFact_resolver&) = delete;

  bool is_registered(gname name) const
  {
    uint32_t hash = fnv1a32(name.strv());
    return is_registered(hash);
  }

  bool is_registered(const CFact& fact) const
  {
    return is_registered(fact.hash());
  }

  bool is_registered(uint32_t hash) const
  {
    return m_invmap.find(hash) != m_invmap.end();
  }

  void register_name(gname name)
  {
    CFact fact(name, 0, false);

    auto it = m_invmap.emplace(fact.hash(), name);
    if (it.second)
    {
      insert_sorted(m_list, name);
    }
  }

  void register_name(std::string_view name)
  {
    register_name(gname(name));
  }

  gname resolve(const CFact& fact) const
  {
    return resolve(fact.hash());
  }

  gname resolve(uint32_t hash) const
  {
    auto it = m_invmap.find(hash);
    if (it != m_invmap.end())
      return it->second;
    return gname(fmt::format("<unknown_fact:{:08X}>", hash));
  }

  const std::vector<gname>& get_sorted_names() const { return m_list; }

  void feed(const std::vector<gname>& names);

protected:
  CFact_resolver() = default;
  ~CFact_resolver() = default;

  std::vector<gname> m_list;
  std::unordered_map<uint32_t, gname> m_invmap;
};


inline gname CFact::name() const
{
  auto& resolver = CFact_resolver::get();
  return resolver.resolve(*this);
}

} // namespace redx

