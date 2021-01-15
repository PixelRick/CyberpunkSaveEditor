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

#pragma pack(push, 1)

struct TweakDBID
{
  // public for its widget

  union {
    struct {
      uint32_t crc;
      uint8_t slen;
      uint8_t uk[2];
      uint8_t _pad;
    };
    uint64_t as_u64 = 0;
  };

  TweakDBID() = default;

  explicit TweakDBID(uint64_t u64)
  {
    as_u64 = u64;
  }

  explicit TweakDBID(std::string_view name)
  {
    as_u64 = 0;
    crc = crc32(name);
    slen = (uint8_t)name.size();
  }

  friend std::istream& operator>>(std::istream& is, TweakDBID& id)
  {
    is >> cbytes_ref(id.as_u64);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const TweakDBID& id)
  {
    os << cbytes_ref(id.as_u64);
    return os;
  }

  std::string name() const;
};


enum class TweakDBIDCategory
{
  All,
  Item,
  Attachment,
  Vehicle,
  Unknown,
};


#pragma pack(pop)

class TweakDBIDResolver
{
  std::vector<std::string> s_full_list;
  std::unordered_map<uint64_t, std::string> s_tdbid_invmap;
  std::unordered_map<uint32_t, std::string> s_crc32_invmap;

  // filtered lists

  std::vector<std::string> s_item_list;
  std::vector<std::string> s_attachment_list;
  std::vector<std::string> s_vehicle_list;
  std::vector<std::string> s_unknown_list;

  TweakDBIDResolver();
  ~TweakDBIDResolver() = default;

public:
  static TweakDBIDResolver& get()
  {
    static TweakDBIDResolver s = {};
    return s;
  }

  TweakDBIDResolver(const TweakDBIDResolver&) = delete;
  TweakDBIDResolver& operator=(const TweakDBIDResolver&) = delete;

public:
  bool is_registered(std::string_view name) const
  {
    return is_registered(TweakDBID(name));
  }

  bool is_registered(const TweakDBID& id) const
  {
    return s_tdbid_invmap.find(id.as_u64) != s_tdbid_invmap.end();
  }

  std::string resolve(const TweakDBID& id) const
  {
    auto it = s_tdbid_invmap.find(id.as_u64);
    if (it != s_tdbid_invmap.end())
      return it->second;
    return fmt::format("<tdbid:{:08X}:{:02X}>", id.crc, id.slen);
  }

  const std::vector<std::string>& sorted_names(TweakDBIDCategory cat = TweakDBIDCategory::All) const
  {
    switch (cat)
    {
      case TweakDBIDCategory::All:          return s_full_list;
      case TweakDBIDCategory::Item:         return s_item_list;
      case TweakDBIDCategory::Attachment:   return s_attachment_list;
      case TweakDBIDCategory::Vehicle:      return s_vehicle_list;
      default: break;
    }
    return s_unknown_list;
  }
};


inline std::string TweakDBID::name() const
{
  auto& tdbid_resolver = TweakDBIDResolver::get();
  return tdbid_resolver.resolve(*this);
}


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
    uint64_t id = FNV1a(name);
    if (s_cname_invmap.find(id) == s_cname_invmap.end())
    {
      s_cname_invmap[id] = name;
      insert_sorted(s_full_list, std::string(name));
    }
  }

  bool is_registered(const CName& cn) const
  {
    return is_registered(cn.as_u64);
  }

  bool is_registered(std::string_view name) const
  {
    uint64_t hash = FNV1a(name);
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

  const std::vector<std::string>& sorted_names() const { return s_full_list; }
};

inline std::string CName::str() const
{
  auto& cname_resolver = CNameResolver::get();
  return cname_resolver.resolve(*this);
}

