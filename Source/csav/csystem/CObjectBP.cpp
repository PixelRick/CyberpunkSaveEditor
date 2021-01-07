#pragma once
#include "CObjectBP.hpp"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

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
        auto bp = std::make_shared<CObjectBP>(CSysName(it.key()));
        auto fields = it.value();
        for (nlohmann::json::iterator field_it = fields.begin(); field_it != fields.end(); ++field_it)
        {
          bp->register_field(CSysName(field_it.key()), CSysName(field_it.value()));
        }
        m_classmap.emplace(CSysName(it.key()).idx(), bp);
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
        for (auto& field : it.second->field_descs())
        {
          jbp[field.name().str()] = field.ctypename().str();
        }
        db[bp->ctypename().str()] = jbp;
      }

      ofs << db.dump(4);
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
}
