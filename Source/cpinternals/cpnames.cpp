#pragma once
#include "cpnames.hpp"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>


TweakDBIDResolver::TweakDBIDResolver()
{
  std::ifstream ifs;
  ifs.open("db/TweakDBIDs.json");
  if (ifs.is_open())
  {
    nlohmann::json db;
    try
    {
      ifs >> db;
      s_namelist.reserve(db.size());
      for (auto& item : db)
        s_namelist.emplace_back(item.get<std::string>());
    }
    catch (std::exception& e)
    {
      std::wostringstream oss;
      oss << L"db/TweakDBIDs.json has an unexpected content" << std::endl;
      oss << e.what();
      MessageBox(0, oss.str().c_str(), L"corrupt resource file", 0);
    }
  }
  else
  {
    MessageBox(0, L"db/TweakDBIDs.json is missing", L"missing resource file", 0);
  }
  for (auto& n : s_namelist)
  {
    TweakDBID id(n);
    s_tdbid_invmap[id.as_u64] = n;
    s_crc32_invmap[id.crc] = n;
  }
  std::sort(s_namelist.begin(), s_namelist.end());
}

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
      s_namelist.reserve(db.size());
      for (auto& item : db)
        s_namelist.emplace_back(item.get<std::string>());
    }
    catch (std::exception&)
    {
      MessageBox(0, L"db/CNames.json has an unexpected content", L"corrupt resource file", 0);
    }
  }
  else
  {
    MessageBox(0, L"db/CNames.json is missing", L"missing resource file", 0);
  }
  for (auto& n : s_namelist)
  {
    CName id(n);
    s_cname_invmap[id.as_u64] = n;
  }
  std::sort(s_namelist.begin(), s_namelist.end());
}

