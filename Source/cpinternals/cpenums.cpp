#pragma once
#include "cpenums.hpp"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

CEnumList::CEnumList()
{
  std::ifstream ifs;
  ifs.open("db/CEnums.json");
  if (ifs.is_open())
  {
    nlohmann::json db;
    try
    {
      ifs >> db;
      for (nlohmann::json::iterator it = db.begin(); it != db.end(); ++it)
      {
        std::cout << it.key() << " : " << it.value() << "\n";
        auto members_sptr = std::make_shared<enum_members_t>();
        auto& members = it.value();
        members_sptr->assign(members.begin(), members.end());
        s_list[it.key()] = members_sptr;
      }
    }
    catch (std::exception& e)
    {
      std::wostringstream oss;
      oss << L"db/CEnums.json has an unexpected content" << std::endl;
      oss << e.what();
      MessageBox(0, oss.str().c_str(), L"corrupt resource file", 0);
    }
  }
  else
  {
    MessageBox(0, L"db/CEnums.json is missing", L"missing resource file", 0);
  }
}

