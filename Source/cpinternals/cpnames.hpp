#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include "utils.hpp"

/////////////////////////////////////////
// TweakDBID
/////////////////////////////////////////

#pragma pack(push, 1)

struct TweakDBID
{
  union {
    struct {
      uint32_t crc;
      uint8_t slen;
      uint8_t uk[2];
      uint8_t _pad;
    };
    uint64_t as_u64;
  };

  TweakDBID() = default;

  explicit TweakDBID(std::string_view name)
  {
    as_u64 = 0;
    crc = crc32(name);
    slen = (uint8_t)name.size();
  }

  template <typename IStream>
  friend IStream& operator>>(IStream& is, TweakDBID& id)
  {
    is.read((char*)&id.as_u64, 8);
    return is;
  }

  template <typename OStream>
  friend OStream& operator<<(OStream& os, const TweakDBID& id)
  {
    os.write((char*)&id.as_u64, 8);
    return os;
  }

  friend std::istream& operator>>(std::istream& is, TweakDBID& id)
  {
    is.read((char*)&id.as_u64, 8);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const TweakDBID& id)
  {
    os.write((char*)&id.as_u64, 8);
    return os;
  }

  std::string name() const;
};

#pragma pack(pop)

class TweakDBIDResolver
{
  std::vector<std::string> s_namelist;
  std::map<uint64_t, std::string> s_tdbid_invmap;
  std::map<uint32_t, std::string> s_crc32_invmap;

  TweakDBIDResolver();
  ~TweakDBIDResolver() = default;

public:
  static TweakDBIDResolver& get()
  {
    static TweakDBIDResolver s;
    return s;
  }

  TweakDBIDResolver(const TweakDBIDResolver&) = delete;
  TweakDBIDResolver& operator=(const TweakDBIDResolver&) = delete;

public:
  bool is_known(std::string_view name) const
  {
    return is_known(TweakDBID(name));
  }

  bool is_known(const TweakDBID& id) const
  {
    return s_tdbid_invmap.find(id.as_u64) != s_tdbid_invmap.end();
  }

  std::string resolve(const TweakDBID& id) const
  {
    auto it = s_tdbid_invmap.find(id.as_u64);
    if (it != s_tdbid_invmap.end())
      return it->second;
    std::stringstream ss;
    ss << "<tdbid:" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << id.as_u64 << ">";
    return ss.str();
  }

  const std::vector<std::string>& sorted_names() const { return s_namelist; }
};


inline std::string TweakDBID::name() const
{
  auto& tdbid_resolver = TweakDBIDResolver::get();
  return tdbid_resolver.resolve(*this);
}


/////////////////////////////////////////
// CName
/////////////////////////////////////////

#pragma pack(push, 1)

struct CName
{
  uint64_t as_u64;

  CName() = default;

  explicit CName(std::string_view name)
  {
    as_u64 = FNV1a(name);
  }

  friend std::istream& operator>>(std::istream& is, CName& id)
  {
    is.read((char*)&id.as_u64, 8);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const CName& id)
  {
    os.write((char*)&id.as_u64, 8);
    return os;
  }

  std::string name() const;
};

#pragma pack(pop)

class CNameResolver
{
  std::vector<std::string> s_namelist;
  std::map<uint64_t, std::string> s_cname_invmap;

  CNameResolver();
  ~CNameResolver() = default;

public:
  static CNameResolver& get()
  {
    static CNameResolver s;
    return s;
  }

  CNameResolver(const CNameResolver&) = delete;
  CNameResolver& operator=(const CNameResolver&) = delete;

public:
  bool is_known(std::string_view name) const
  {
    return is_known(CName(name));
  }

  bool is_known(const CName& id) const
  {
    return s_cname_invmap.find(id.as_u64) != s_cname_invmap.end();
  }

  std::string resolve(const CName& id) const
  {
    auto it = s_cname_invmap.find(id.as_u64);
    if (it != s_cname_invmap.end())
      return it->second;
    std::stringstream ss;
    ss << "<cname:" << std::hex << std::setw(16) << std::setfill('0') << std::uppercase << id.as_u64 << ">";
    return ss.str();
  }

  const std::vector<std::string>& sorted_names() const { return s_namelist; }
};

inline std::string CName::name() const
{
  auto& cname_resolver = CNameResolver::get();
  return cname_resolver.resolve(*this);
}

