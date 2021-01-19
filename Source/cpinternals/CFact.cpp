#pragma once
#include <cpinternals/CFact.hpp>

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

void to_json(nlohmann::json& j, const CSysName& csn)
{
  j = csn.str();
}

void from_json(const nlohmann::json& j, CSysName& csn)
{
  csn = CSysName(j.get<std::string>());
}

namespace CP {

CFact::CFact(CSysName name, uint32_t value)
{
  m_hash = fnv1a32(name.str());
  CFactResolver::get().insert(name);
  // todo: export on newly discovered name..
}

CSysName CFact::name() const
{
  auto& resolver = CFactResolver::get();
  return resolver.resolve(*this);
}

void CFact::name(CSysName name) 
{
  auto& resolver = CFactResolver::get();
  m_hash = fnv1a32(name.str());
  resolver.insert(name);
}

CFactResolver::CFactResolver()
{
  std::ifstream ifs;
  ifs.open("db/CFactsDB.json");
  if (ifs.is_open())
  {
    nlohmann::json db;
    try
    {
      ifs >> db;
      db.get_to(m_list);
    }
    catch (std::exception& e)
    {
      std::wostringstream oss;
      oss << L"db/CFactsDB.json has unexpected content" << std::endl;
      oss << e.what();
      MessageBox(0, oss.str().c_str(), L"corrupt resource file", 0);
    }
  }
  else
  {
    MessageBox(0, L"db/CFactsDB.json is missing", L"missing resource file", 0);
  }

  for (auto& n : m_list)
  {
    uint32_t hash = fnv1a32(n.str());
    m_invmap[hash] = n;
  }

  std::sort(m_list.begin(), m_list.end());
}

} // namespace CP

