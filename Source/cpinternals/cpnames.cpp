#pragma once
#include "cpnames.hpp"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

CNameResolver::CNameResolver()
{
  std::ifstream ifs;
  ifs.open("db/CNames.json");
  if (ifs.is_open())
  {
    nlohmann::json db;
    try
    {
      ifs >> db;
      s_full_list.reserve(db.size());
      for (auto& jelem : db)
        s_full_list.emplace_back(jelem.get<std::string>());
    }
    catch (std::exception&)
    {
      MessageBox(0, L"db/CNames.json has unexpected content", L"corrupt resource file", 0);
    }
  }
  else
  {
    MessageBox(0, L"db/CNames.json is missing", L"CObjectBPs", 0);
  }
  for (auto& n : s_full_list)
  {
    s_cname_invmap[fnv1a64(n)] = n;
    s_cname_invmap32[fnv1a32(n)] = n;
  }
  std::sort(s_full_list.begin(), s_full_list.end());
}

CName::CName(std::string_view name)
{
  as_u64 = fnv1a64(name);
  CNameResolver::get().register_name(name);
  // todo: export on newly discovered name..
}

CName::CName(uint64_t hash)
{
  as_u64 = hash;
}

