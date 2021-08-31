#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "redx/common.hpp"

namespace redx {

struct CEnum_member
{
  CEnum_member() = default;

  CEnum_member(gname name, uint32_t value)
    : m_name(name), m_value(value)
  {
  }

  gname name() const
  {
    return m_name;
  }

  uint32_t value() const
  {
    return m_value;
  }

  friend void to_json(nlohmann::json& j, const CEnum_member& x);
  friend void from_json(const nlohmann::json& j, CEnum_member& x);

protected:
  gname m_name;
  uint32_t m_value;
};


struct CEnum_desc
{
  gname name() const
  {
    return m_name;
  }

  std::vector<CEnum_member>& members()
  {
    return m_members;
  }

  friend void to_json(nlohmann::json& j, const CEnum_desc& x);
  friend void from_json(const nlohmann::json& j, CEnum_desc& x);

protected:
  gname m_name;
  std::vector<CEnum_member> m_members;
};


struct CEnum_resolver
{
  using enum_desc_sptr = std::shared_ptr<CEnum_desc>;

  static CEnum_resolver& get()
  {
    static CEnum_resolver s = {};
    return s;
  }

  CEnum_resolver(const CEnum_resolver&) = delete;
  CEnum_resolver& operator=(const CEnum_resolver&) = delete;

  bool is_registered(gname enum_name) const
  {
    return m_enums_map.find(enum_name) != m_enums_map.end();
  }

  enum_desc_sptr get_enum(gname enum_name) const
  {
    auto it = m_enums_map.find(enum_name);
    if (it != m_enums_map.end())
      return it->second;
    return nullptr;
  }

  friend void to_json(nlohmann::json& j, const CEnum_resolver& x);
  friend void from_json(const nlohmann::json& j, CEnum_resolver& x);

protected:
  CEnum_resolver() = default;
  ~CEnum_resolver() = default;

  std::unordered_map<gname, enum_desc_sptr> m_enums_map;
};

} // namespace redx

