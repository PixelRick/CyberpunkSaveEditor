#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <fmt/format.h>
#include <utils.hpp>
#include <csav/csystem/fwd.hpp>
#include <csav/csystem/CPropertyFactory.hpp>
#include <csav/csystem/CStringPool.hpp>

class CFieldDesc
{
  CSysName m_name;
  CSysName m_ctypename;

  std::function<CPropertySPtr()> m_prop_creator;

public:
  CFieldDesc(CSysName name, CSysName ctypename)
    : m_name(name), m_ctypename(ctypename)
  {
    m_prop_creator = CPropertyFactory::get().get_creator(ctypename);
  }

  CSysName name() const { return m_name; }
  CSysName ctypename() const { return m_ctypename; }

  CPropertySPtr create_prop() const { return m_prop_creator(); }
};

class CObjectBP
{
  CSysName m_ctypename;
  std::vector<CFieldDesc> m_field_descs;

public:
  explicit CObjectBP(CSysName ctypename)
    : m_ctypename(ctypename) {}

  CSysName ctypename() const { return m_ctypename; }

  const std::vector<CFieldDesc>& field_descs() const { return m_field_descs; }

  void register_field(CSysName field_name, CSysName ctypename)
  {
    for (auto& field : m_field_descs)
    {
      if (field.name() == field_name)
      {
        if (field.ctypename() != ctypename)
          throw std::runtime_error("CObjectBP identified field has different type than the registered one");
        return;
      }
    }
    m_field_descs.emplace_back(field_name, ctypename);
  }
};

class CObjectBPList
{
private:
  std::unordered_map<uint32_t, CObjectBPSPtr> m_classmap;

  // filtered lists

  CObjectBPList();
  ~CObjectBPList();

public:
  CObjectBPList(const CObjectBPList&) = delete;
  CObjectBPList& operator=(const CObjectBPList&) = delete;

  static CObjectBPList& get()
  {
    static CObjectBPList s = {};
    return s;
  }

public:
  // always returns a class, so that unknown ones can be configured
  CObjectBPSPtr get_or_make_bp(CSysName objtype)
  {
    auto it = m_classmap.find(objtype.idx());
    if (it != m_classmap.end())
      return it->second;
    it = m_classmap.emplace(objtype.idx(), std::make_shared<CObjectBP>(objtype)).first;
    return it->second;
  }
};

