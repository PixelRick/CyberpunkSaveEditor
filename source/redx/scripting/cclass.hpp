#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <algorithm>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "fwd.hpp"
#include "redx/utils.hpp"
#include "iproperty.hpp"
#include "cproperty_factory.hpp"
#include "CStringPool.hpp"


struct CFieldDesc
{
  gname name;
  gname ctypename;

  CFieldDesc() = default;

  CFieldDesc(gname name, gname ctypename)
    : name(name), ctypename(ctypename) {}
};


//static_assert(sizeof(CFieldDesc) == sizeof(uint64_t));


class CFieldBP
{
  CFieldDesc m_desc;

  std::function<CPropertyUPtr(CPropertyOwner*)> m_prop_creator;

public:
  CFieldBP(CFieldDesc desc)
    : m_desc(desc)
  {
    m_prop_creator = CPropertyFactory::get().get_creator(desc.ctypename);
  }

  CFieldDesc desc() const { return m_desc; }

  gname name() const { return m_desc.name; }
  gname ctypename() const { return m_desc.ctypename; }

  CPropertyUPtr create_prop(CPropertyOwner* owner) const { return std::move(m_prop_creator(owner)); }
};


class CObjectBP
{
  gname m_ctypename;
  std::vector<CFieldBP> m_field_bps;
  CObjectBPSPtr m_parent;
  std::vector<std::weak_ptr<CObjectBP>> m_children;

public:
  explicit CObjectBP(gname ctypename)
    : m_ctypename(ctypename) {}

  CObjectBP(gname ctypename, CObjectBPSPtr parent, const std::vector<CFieldDesc>& fdescs)
    : m_ctypename(ctypename), m_parent(parent)
  {
    if (m_parent)
    {
      const auto& parent_fields_bps = m_parent->m_field_bps;
      m_field_bps.assign(parent_fields_bps.begin(), parent_fields_bps.end());
    }
    for (const auto& fdesc : fdescs)
      m_field_bps.emplace_back(fdesc);
  }

  gname ctypename() const { return m_ctypename; }
  CObjectBPSPtr parent() const { return m_parent; }

  const std::vector<std::weak_ptr<CObjectBP>>& children() const { return m_children; }

  const std::vector<CFieldBP>& field_bps() const { return m_field_bps; }

  void add_child(const CObjectBPSPtr& child)
  {
    m_children.emplace_back(child);
  }

};

class CObjectBPList
{
private:
  std::unordered_map<gname, CObjectBPSPtr> m_classmap;

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
  // always returns a class, so that new ones can be configured
  CObjectBPSPtr get_or_make_bp(gname objtype)
  {
    auto it = m_classmap.find(objtype);
    if (it != m_classmap.end())
      return it->second;
    it = m_classmap.emplace(objtype, std::make_shared<CObjectBP>(objtype)).first;
    return it->second;
  }
};

