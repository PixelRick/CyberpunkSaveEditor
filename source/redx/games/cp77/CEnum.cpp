#include "CEnum.hpp"

#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace redx {

void to_json(nlohmann::json& j, const CEnum_member& x)
{
  //j = {{"name", x.m_name}, {"value", x.m_value}};
  j = x.m_name;
}

void from_json(const nlohmann::json& j, CEnum_member& x)
{
  //x.m_name = gname(j["name"].get<std::string>());
  //x.m_value = j["value"].get<uint32_t>();
  x.m_name = gname(j.get<std::string>());
}

void to_json(nlohmann::json& j, const CEnum_desc& x)
{
  //j = {{"name", x.m_name}, {"members", x.m_members}};
  j = x.m_members;
}

void from_json(const nlohmann::json& j, CEnum_desc& x)
{
  //x.m_name = gname(j["name"].get<std::string>());
  //x.m_members = j["members"].get<std::vector<CEnum_member>>();
  j.get_to(x.m_members);
}

void to_json(nlohmann::json& j, const CEnum_resolver& x)
{
  // todo
}

void from_json(const nlohmann::json& j, CEnum_resolver& x)
{
  x.m_enums_map.clear();
  for (auto it = j.begin(); it != j.end(); ++it)
  {
    auto new_desc = std::make_shared<CEnum_desc>();
    it.value().get_to(*new_desc);
    gname name(it.key());
    x.m_enums_map.emplace(name, new_desc);
  }
}

} // namespace redx

