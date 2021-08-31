#pragma once
#include <inttypes.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include "redx/common.hpp"

namespace redx {

/////////////////////////////////////////
// TweakDBID
/////////////////////////////////////////

struct TweakDBID
{
  TweakDBID() = default;
  TweakDBID(const TweakDBID&) = default;

  explicit TweakDBID(uint32_t crc, size_t slen)
    : as_u64(0)
  {
    if (slen > 0xFF)
      throw std::length_error("TweakDBID's length overflow");
    this->crc = crc;
    this->slen = static_cast<uint8_t>(slen);
  }

  explicit TweakDBID(uint64_t u64)
    : as_u64(u64)
  {
  }

  explicit TweakDBID(std::string_view name, bool add_to_resolver = true);

  explicit TweakDBID(const char* name, bool add_to_resolver = true)
    : TweakDBID(std::string_view(name), add_to_resolver)
  {
  }

  explicit TweakDBID(gname name, bool add_to_resolver = true)
    : TweakDBID(name.strv(), add_to_resolver)
  {
  }

  TweakDBID& operator=(const TweakDBID&) = default;

  TweakDBID& operator+=(const TweakDBID& rhs)
  {
    crc = crc32_combine(crc, rhs.crc, rhs.slen);
    slen += rhs.slen;
    return *this;
  }

  TweakDBID& operator+=(std::string_view name)
  {
    crc = crc32_str(name, crc);
    if (slen + name.size() > 0xFF)
      throw std::length_error("TweakDBID's length overflow");
    slen += static_cast<uint8_t>(name.size());
    return *this;
  }

  template <typename T>
  TweakDBID operator+(const T& rhs)
  {
    TweakDBID ret(*this);
    ret += rhs;
    return ret;
  }

  friend streambase& operator<<(streambase& ar, TweakDBID& id)
  {
    constexpr uint64_t mask = (1ull << 40) - 1;
    id.as_u64 &= mask;
    return ar << id.as_u64;
  }

  friend bool operator<(const TweakDBID& a, const TweakDBID& b)
  {
    return a.as_u64 < b.as_u64;
  }

  gname name() const;

#pragma pack(push, 1)

  union
  {
    struct
    {
      uint32_t crc;
      uint8_t slen;
      uint8_t tdboff1[2];
      uint8_t tdboff0;
    };

    uint64_t as_u64 = 0;
  };

#pragma pack(pop)
};

static_assert(sizeof(TweakDBID) == 8);


enum class TweakDBID_category
{
  All,
  Item,
  Attachment,
  Vehicle,
  Unknown,
};

/////////////////////////////////////////
// resolver
/////////////////////////////////////////

struct TweakDBID_resolver
{
  static TweakDBID_resolver& get()
  {
    static TweakDBID_resolver s = {};
    return s;
  }

  TweakDBID_resolver(const TweakDBID_resolver&) = delete;
  TweakDBID_resolver& operator=(const TweakDBID_resolver&) = delete;

  bool is_registered(std::string_view name) const
  {
    return is_registered(TweakDBID(name, false));
  }

  bool is_registered(const TweakDBID& id) const
  {
    return m_tdbid_invmap.find(id.as_u64) != m_tdbid_invmap.end();
  }

  void register_name(gname name);

  void register_name(std::string_view name)
  {
    register_name(gname(name));
  }

  gname resolve(const TweakDBID& id) const
  {
    auto it = m_tdbid_invmap.find(id.as_u64);
    if (it != m_tdbid_invmap.end())
      return it->second;
    return gname(fmt::format("<tdbid:{:08X}:{:02X}>", id.crc, id.slen));
  }

  const std::vector<gname>& sorted_names(TweakDBID_category cat = TweakDBID_category::Unknown) const
  {
    switch (cat)
    {
      case TweakDBID_category::All:          return m_full_list;
      case TweakDBID_category::Item:         return m_item_list;
      case TweakDBID_category::Attachment:   return m_attachment_list;
      case TweakDBID_category::Vehicle:      return m_vehicle_list;
      default: break;
    }
    return m_unknown_list;
  }

  void feed(const std::vector<gname>& names);

protected:
  TweakDBID_resolver() = default;
  ~TweakDBID_resolver() = default;

  std::vector<gname> m_full_list;

  std::unordered_map<uint64_t, gname> m_tdbid_invmap;
  std::unordered_map<uint32_t, gname> m_crc32_invmap;

  // filtered lists

  std::vector<gname> m_item_list;
  std::vector<gname> m_attachment_list;
  std::vector<gname> m_vehicle_list;
  std::vector<gname> m_unknown_list;
};


inline gname TweakDBID::name() const
{
  auto& resolver = TweakDBID_resolver::get();
  return resolver.resolve(*this);
}

} // namespace redx

