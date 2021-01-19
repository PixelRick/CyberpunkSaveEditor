#pragma once
#include "cpnames.hpp"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

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
      s_full_list.reserve(db.size());
      for (auto& jelem : db)
      {
        std::string name = jelem.get<std::string>();
        s_full_list.emplace_back(jelem.get<std::string>());
        if (name.rfind("Ammo.", 0) == 0)
        {
          s_item_list.emplace_back(name);
        }
        else if (name.rfind("Items.", 0) == 0)
        {
          s_item_list.emplace_back(name);
          auto rar_name = name + "_Rare";
          //s_item_list.emplace_back(rar_name);
          s_full_list.emplace_back(rar_name);
          auto epc_name = name + "_Epic";
          //s_item_list.emplace_back(epc_name);
          s_full_list.emplace_back(epc_name);
          auto leg_name = name + "_Legendary";
          //s_item_list.emplace_back(leg_name);
          s_full_list.emplace_back(leg_name);
        }
        else if (name.rfind("AttachmentSlots.", 0) == 0)
        {
          s_attachment_list.emplace_back(name);
          auto rar_name = name + "_Rare";
          //s_attachment_list.emplace_back(rar_name);
          s_full_list.emplace_back(rar_name);
          auto epc_name = name + "_Epic";
          //s_attachment_list.emplace_back(epc_name);
          s_full_list.emplace_back(epc_name);
          auto leg_name = name + "_Legendary";
          //s_attachment_list.emplace_back(leg_name);
          s_full_list.emplace_back(leg_name);
        }
        else if (name.rfind("Vehicle.v_", 0) == 0)
        {
          s_vehicle_list.emplace_back(name);
        }
        else if (name.rfind("Vehicle.av_", 0) == 0)
        {
          s_vehicle_list.emplace_back(name);
        }
        else
        {
          s_unknown_list.emplace_back(name);
        }
      }
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

  for (auto& n : s_full_list)
  {
    TweakDBID id(n);
    s_tdbid_invmap[id.as_u64] = n;
    s_crc32_invmap[id.crc] = n;
  }
  
  std::sort(s_full_list.begin(), s_full_list.end());
  std::sort(s_item_list.begin(), s_item_list.end());
  std::sort(s_attachment_list.begin(), s_attachment_list.end());
  std::sort(s_vehicle_list.begin(), s_vehicle_list.end());
  std::sort(s_unknown_list.begin(), s_unknown_list.end());
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

