#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <csav/serializers.hpp>
#include <fmt/format.h>
#include <utils.hpp>

/////////////////////////////////////////
// TweakDBID
/////////////////////////////////////////

#include "tweakdb/TweakDBID.hpp"
using namespace cp;

/////////////////////////////////////////
// CName
/////////////////////////////////////////

struct CName
{
  uint64_t as_u64 = 0;

  CName() = default;

  explicit CName(std::string_view name);

  explicit CName(uint64_t u64);

  friend std::istream& operator>>(std::istream& is, CName& id)
  {
    is >> cbytes_ref(id.as_u64);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const CName& id)
  {
    os << cbytes_ref(id.as_u64);
    return os;
  }

  std::string name() const { return str(); }

  std::string str() const;
};

inline bool operator==(const CName& a, const CName& b)
{
  return a.as_u64 == b.as_u64;
}

inline bool operator!=(const CName& a, const CName& b)
{
  return !(a == b);
}

class CNameResolver
{
  std::vector<std::string> s_full_list;
  std::unordered_map<uint64_t, std::string> s_cname_invmap;
  std::unordered_map<uint32_t, std::string> s_cname_invmap32;

  CNameResolver();
  ~CNameResolver() = default;

public:
  static CNameResolver& get()
  {
    static CNameResolver s = {};
    return s;
  }

  CNameResolver(const CNameResolver&) = delete;
  CNameResolver& operator=(const CNameResolver&) = delete;

public:
  void register_name(std::string_view name)
  {
    uint64_t id = fnv1a64(name);
    if (s_cname_invmap.find(id) == s_cname_invmap.end())
    {
      s_cname_invmap[id] = name;
      s_cname_invmap32[fnv1a32(name)] = name;
      insert_sorted(s_full_list, std::string(name));
    }
  }

  bool is_registered(const CName& cn) const
  {
    return is_registered(cn.as_u64);
  }

  bool is_registered(std::string_view name) const
  {
    uint64_t hash = fnv1a64(name);
    return is_registered(hash);
  }

  bool is_registered(uint64_t hash) const
  {
    return s_cname_invmap.find(hash) != s_cname_invmap.end();
  }

  std::string resolve(const CName& id) const
  {
    return resolve(id.as_u64);
  }

  std::string resolve(uint64_t hash) const
  {
    auto it = s_cname_invmap.find(hash);
    if (it != s_cname_invmap.end())
      return it->second;
    return fmt::format("<cname:{:016X}>", hash);
  }

  std::string resolve(uint32_t hash32) const
  {
    auto it = s_cname_invmap32.find(hash32);
    if (it != s_cname_invmap32.end())
      return it->second;
    return fmt::format("<cname32:{:08X}>", hash32);
  }

  const std::vector<std::string>& sorted_names() const { return s_full_list; }
};

inline std::string CName::str() const
{
  auto& cname_resolver = CNameResolver::get();
  return cname_resolver.resolve(*this);
}

