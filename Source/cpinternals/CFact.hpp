#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

#include <fmt/format.h>
#include <utils.hpp>
#include <csav/csystem/CStringPool.hpp>

namespace CP {

/////////////////////////////////////////
// CFact
/////////////////////////////////////////

struct CFact
{
  CFact() = default;

  CFact(uint32_t hash, uint32_t value)
    : m_hash(hash), m_value(value)
  {
  }

  CFact(CSysName name, uint32_t value);

  CSysName name() const;

  uint32_t hash() const
  {
    return m_hash;
  }

  void hash(uint32_t hash)
  {
    m_hash = hash;
  }

  void name(CSysName name);

  uint32_t value() const
  {
    return m_value;
  }

  void value(uint32_t value)
  {
    m_value = value;
  }

  friend inline bool operator==(const CFact& a, const CFact& b)
  {
    return a.m_hash == b.m_hash && a.m_value == b.m_value;
  }

protected:
  uint32_t m_hash = 0;
  uint32_t m_value = 0;
};


inline bool operator!=(const CFact& a, const CFact& b)
{
  return !(a == b);
}


struct CFactResolver
{
  static CFactResolver& get()
  {
    static CFactResolver s = {};
    return s;
  }

  CFactResolver(const CFactResolver&) = delete;
  CFactResolver& operator=(const CFactResolver&) = delete;

  void insert(CSysName name)
  {
    uint32_t id = FNV1a32(name.str());

    auto it = m_invmap.emplace(id, name);
    if (it.second)
    {
      insert_sorted(m_list, name);
    }
  }

  bool is_registered(const CFact& fact) const
  {
    return is_registered(fact.hash());
  }

  bool is_registered(CSysName name) const
  {
    uint32_t hash = FNV1a32(name.str());
    return is_registered(hash);
  }

  bool is_registered(uint32_t hash) const
  {
    return m_invmap.find(hash) != m_invmap.end();
  }

  CSysName resolve(const CFact& fact) const
  {
    return resolve(fact.hash());
  }

  CSysName resolve(uint32_t hash) const
  {
    auto it = m_invmap.find(hash);
    if (it != m_invmap.end())
      return it->second;
    return CSysName(fmt::format("<unknown_fact:{:08X}>", hash));
  }

  const std::vector<CSysName>& sorted_names() const { return m_list; }

protected:
  CFactResolver();
  ~CFactResolver() = default;

  std::vector<CSysName> m_list;
  std::unordered_map<uint32_t, CSysName> m_invmap;
};

} // namespace CP

