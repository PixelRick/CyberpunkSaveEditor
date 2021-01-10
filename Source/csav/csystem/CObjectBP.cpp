#pragma once
#include "CObjectBP.hpp"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

void to_json(nlohmann::json& j, const CFieldDesc& p)
{
  j = {{"name", p.name.str()}, {"ctypename", p.ctypename.str()}};
}

void from_json(const nlohmann::json& j, CFieldDesc& p)
{
  p.name = CSysName(j.at("name").get<std::string>());
  p.ctypename = CSysName(j.at("ctypename").get<std::string>());
}


CObjectBPList::CObjectBPList()
{
  std::ifstream ifs;
  ifs.open("db/CObjectBPs.json");
  if (ifs.is_open())
  {
    nlohmann::json db;
    try
    {
      ifs >> db;
      for (nlohmann::json::iterator it = db.begin(); it != db.end(); ++it)
      {
        auto fields = it.value();
        std::vector<CFieldDesc> fdescs;
        it.value().get_to(fdescs);
        auto bp = std::make_shared<CObjectBP>(CSysName(it.key()), fdescs);
        m_classmap.emplace(CSysName(it.key()), bp);
      }
    }
    catch (std::exception& e)
    {
      std::wostringstream oss;
      oss << L"db/CObjectBPs.json has unexpected content" << std::endl;
      oss << e.what();
      MessageBox(0, oss.str().c_str(), L"corrupt resource file", 0);
    }
  }
  else
  {
    MessageBox(0, L"db/CEnums.json is missing", L"couldn't open resource file", 0);
  }
}

CObjectBPList::~CObjectBPList()
{
#ifdef COBJECT_BP_GENERATION
  std::ofstream ofs;
  ofs.open("db/CObjectBPs.json");
  if (ofs.is_open())
  {
    try
    {
      nlohmann::json db;
      for (auto& it : m_classmap)
      {
        const auto& bp = it.second;
        nlohmann::json jbp;
        std::vector<CFieldDesc> fdescs;
        for (auto& field : it.second->field_bps())
          fdescs.emplace_back(field.name(), field.ctypename());
        db[bp->ctypename().str()] = fdescs;
      }

      ofs << db.dump(2);
    }
    catch (std::exception& e)
    {
      std::wostringstream oss;
      oss << L"error occurred during db/CObjectBPs.json update:" << std::endl;
      oss << e.what();
      MessageBox(0, oss.str().c_str(), L"couldn't update resource file", 0);
    }
  }
  else
  {
    MessageBox(0, L"db/CObjectBPs.json couldn't be updated", L"couldn't open resource file", 0);
  }
#endif
}
