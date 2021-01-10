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
#include <fmt/format.h>
#include <utils.hpp>
#include <nlohmann/json.hpp>
#include <csav/csystem/fwd.hpp>
#include <csav/csystem/CPropertyBase.hpp>
#include <csav/csystem/CPropertyFactory.hpp>
#include <csav/csystem/CStringPool.hpp>

//#define COBJECT_BP_GENERATION

// little tool to retrieve the real order of fields
class field_order_graph
{
  std::unordered_map<CSysName, uint16_t> name_to_idx;
  std::vector<CSysName> idx_to_name;

  std::vector<std::set<uint16_t>> adjs;
  // adj[0].contains(1)  ==>  0 < 1

  void topo_sort_rec(std::vector<bool>& visited, std::vector<uint16_t>& stack, uint16_t i)
  {
    visited[i] = true;
    const auto& adj_i = adjs[i];
    for (auto it = adj_i.begin(); it != adj_i.end(); ++it)
    {
      if (!visited[*it])
        topo_sort_rec(visited, stack, *it);
    }
    stack.push_back(i);
  }

public:

  // ok let's CHECK for back edges :)

  std::vector<CSysName> topo_sort()
  {
    const uint16_t elts_cnt = (uint16_t)idx_to_name.size();
    std::vector<bool> visited(elts_cnt);
    std::vector<uint16_t> stack;

    for (uint16_t i = 0; i < elts_cnt; ++i)
    {
      if (!visited[i])
        topo_sort_rec(visited, stack, i);
    }

    // check that there is no conflicting relation (cycles)
    std::set<uint16_t> preds;
    for (auto it = stack.rbegin(); it != stack.rend(); ++it)
    {
      auto& adj = adjs[*it];
      for (auto& a : adj)
      {
        if (preds.find(a) != preds.end())
          throw std::runtime_error("field_order_graph is over constrained, not cool :(");
        preds.emplace(*it);
      }
    }

    std::vector<CSysName> ret;
    while (stack.size())
    {
      ret.push_back(idx_to_name[stack.back()]);
      stack.pop_back();
    }
    return ret;
  }

  void insert_name(const CSysName& name)
  {
    // todo: add size check (uint16_t max)
    size_t idx = idx_to_name.size();
    if (name_to_idx.emplace(name, (uint16_t)idx).second)
    {
      idx_to_name.push_back(name);
      adjs.emplace_back();
    }
  }

  // returns true if transitive closure has been updated
  bool insert_ordered_list(const std::vector<CSysName>& names)
  {
    bool has_new_constraint = false;
    std::set<uint16_t> adj_set;
    for (auto rit = names.rbegin(); rit != names.rend(); ++rit)
    {
      // todo: add size check (uint16_t max)
      size_t idx = idx_to_name.size();
      auto it = name_to_idx.emplace(*rit, (uint16_t)idx);
      if (it.second)
      {
        idx_to_name.push_back(*rit);
        adjs.emplace_back();
        has_new_constraint = true;
      }
      else
        idx = it.first->second;

      const size_t prev_size = adjs[idx].size();
      adjs[idx].insert(adj_set.begin(), adj_set.end());
      if (prev_size != adjs[idx].size())
        has_new_constraint = true;

      adj_set.emplace((uint16_t)idx);
    }

    return has_new_constraint;
  }
};

struct CFieldDesc
{
  CSysName name;
  CSysName ctypename;

  CFieldDesc() = default;

  CFieldDesc(CSysName name, CSysName ctypename)
    : name(name), ctypename(ctypename) {}
};

static_assert(sizeof(CFieldDesc) == sizeof(uint64_t));


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

  CSysName name() const { return m_desc.name; }
  CSysName ctypename() const { return m_desc.ctypename; }

  CPropertyUPtr create_prop(CPropertyOwner* owner) const { return std::move(m_prop_creator(owner)); }
};

class CObjectBP
{
  CSysName m_ctypename;
  std::vector<CFieldBP> m_field_bps;

#ifdef COBJECT_BP_GENERATION
  field_order_graph m_fograph;
#endif

public:
  explicit CObjectBP(CSysName ctypename)
    : m_ctypename(ctypename) {}

  CObjectBP(CSysName ctypename, const std::vector<CFieldDesc>& fdescs)
    : m_ctypename(ctypename)
  {
    // let's consider our db orders correct..
#ifndef COBJECT_BP_GENERATION
    register_partial_field_descs(fdescs);
#endif
    for (const auto& fdesc : fdescs)
    {
#ifdef COBJECT_BP_GENERATION
      m_fograph.insert_name(fdesc.name);
#endif
      // append the field bp
      m_field_bps.emplace_back(fdesc);
    }
  }

  CSysName ctypename() const { return m_ctypename; }

  const std::vector<CFieldBP>& field_bps() const { return m_field_bps; }

  bool register_partial_field_descs(const std::vector<CFieldDesc>& descs)
  {
#ifdef COBJECT_BP_GENERATION
    std::vector<CSysName> ordered_names;
    for (auto& desc : descs)
    {
      ordered_names.push_back(desc.name);

      bool found = false;
      for (auto& fbp : m_field_bps)
      {
        if (fbp.name() == desc.name)
        {
          if (fbp.ctypename() != desc.ctypename)
          {
            throw std::runtime_error( // todo: replace with logged error
              fmt::format(
                "CObjectBP: a {}'s field bp has different ctype ({}) than newly registered one ({}).",
                fbp.ctypename().str(), desc.ctypename.str()));
            return false;
          }
          found = true;
          break;
        }
      }
      if (!found)
      {
        m_field_bps.emplace_back(desc);
      }
    }

    if (m_fograph.insert_ordered_list(ordered_names))
    {
      auto ordered = m_fograph.topo_sort();
      std::vector<CFieldBP> new_field_bps;
      for (auto& name : ordered)
      {
        auto it = std::find_if(m_field_bps.begin(), m_field_bps.end(),
          [name](const CFieldBP& fbp){ return fbp.name() == name; });
        new_field_bps.push_back(std::move(*it));
        m_field_bps.erase(it);
      }
      m_field_bps = std::move(new_field_bps);
    }

#endif
    return true;
  }
};

class CObjectBPList
{
private:
  std::unordered_map<CSysName, CObjectBPSPtr> m_classmap;

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
    auto it = m_classmap.find(objtype);
    if (it != m_classmap.end())
      return it->second;
    it = m_classmap.emplace(objtype, std::make_shared<CObjectBP>(objtype)).first;
    return it->second;
  }
};

