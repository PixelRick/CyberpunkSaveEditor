#pragma once
#include <inttypes.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fmt/format.h>
#include "cpinternals/common.hpp"

namespace cp {

/////////////////////////////////////////
// CName
/////////////////////////////////////////

struct CName
{
  CName() = default;
  CName(const CName&) = default;

  constexpr explicit CName(uint64_t u64)
    : as_u64(u64)
  {
  }

  explicit CName(std::string_view name, bool add_to_resolver = true);

  explicit CName(const char* name, bool add_to_resolver = true)
    : CName(std::string_view(name), add_to_resolver)
  {
  }

  explicit CName(gname name, bool add_to_resolver = true)
    : CName(name.strv(), add_to_resolver)
  {
  }

  CName& operator=(const CName&) = default;

  friend iarchive& operator<<(iarchive& ar, CName& id);

  friend bool operator==(const CName& a, const CName& b)
  {
    return a.as_u64 == b.as_u64;
  }

  friend bool operator!=(const CName& a, const CName& b)
  {
    return !(a == b);
  }

  friend bool operator<(const CName& a, const CName& b)
  {
    return a.as_u64 < b.as_u64;
  }

  gname name() const;

  uint64_t as_u64 = 0;
};

static_assert(sizeof(CName) == 8);

/////////////////////////////////////////
// resolver
/////////////////////////////////////////

struct CName_resolver
{
  static CName_resolver& get()
  {
    static CName_resolver s = {};
    return s;
  }

  CName_resolver(const CName_resolver&) = delete;
  CName_resolver& operator=(const CName_resolver&) = delete;

  bool is_registered(std::string_view name) const
  {
    return is_registered(CName(name, false));
  }

  bool is_registered(const CName& cn) const
  {
    return is_registered(cn.as_u64);
  }

  bool is_registered(uint64_t hash) const
  {
    return m_invmap.find(hash) != m_invmap.end();
  }

  bool is_registered_32(uint32_t hash) const
  {
    return m_invmap.find(hash) != m_invmap.end();
  }

  void register_name(gname name);

  void register_name(std::string_view name)
  {
    register_name(gname(name));
  }

  gname resolve(const CName& id) const
  {
    return resolve(id.as_u64);
  }

  gname resolve(uint64_t hash) const
  {
    auto it = m_invmap.find(hash);
    if (it != m_invmap.end())
      return it->second;
    return gname(fmt::format("<cname:{:016X}>", hash));
  }

  gname resolve_32(uint32_t hash32) const
  {
    auto it = m_invmap_32.find(hash32);
    if (it != m_invmap_32.end())
      return it->second;
    return gname(fmt::format("<cname32:{:08X}>", hash32));
  }

  const std::vector<gname>& sorted_names() const
  {
    return m_full_list;
  }

  void feed(const std::vector<gname>& names);

protected:
  CName_resolver() = default;
  ~CName_resolver() = default;

  std::vector<gname> m_full_list;
  std::unordered_map<uint64_t, gname> m_invmap;
  std::unordered_map<uint32_t, gname> m_invmap_32;
};


inline gname CName::name() const
{
  auto& resolver = CName_resolver::get();
  return resolver.resolve(*this);
}

inline iarchive& operator<<(iarchive& ar, CName& id)
{
  if (ar.flags() & armanip::cnamehash)
  {
    return ar << id.as_u64;
  }

  if (ar.is_reader())
  {
    std::string s;
    ar << s;
    id = CName(s);
  }
  else
  {
    auto& resolver = CName_resolver::get();
    if (!resolver.is_registered(id))
    {
      throw std::runtime_error("trying to serialize an unknown cname as string!");
    }
    std::string s = id.name().string();
    ar << s;
  }

  return ar;
}

} // namespace cp

