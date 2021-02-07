#pragma once
#include <vector>
#include <memory>
#include <fstream>
#include <fmt/format.h>

#include "fwd.hpp"
#include "CStringPool.hpp"

class CSystemSerCtx
{
  friend class CSystem;
  friend class archive_test;

protected:
  std::vector<CObjectSPtr> m_objects;
  std::unordered_map<uintptr_t, uint32_t> m_ptrmap;

  std::ofstream m_logfile;

public:
  // TODO: include a logging lib!!

  CSystemSerCtx()
  {
    //m_logfile.open(fmt::format("CSystemSerLOG_{:08X}.txt", (uint32_t)(uint64_t)this));
  }

  ~CSystemSerCtx()
  {
    //m_logfile.close();
  }

  void log(std::string_view s)
  {
    //m_logfile << s << std::endl;
  }

  CStringPool strpool;

public:
  void rebuild_handlemap()
  {
    m_ptrmap.clear();
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
      const uintptr_t key = (uintptr_t)m_objects[i].get();
      m_ptrmap.emplace(key, (uint32_t)i);
    }
  }

  uint32_t to_handle(const CObjectSPtr& obj)
  {
    const uintptr_t key = (uintptr_t)obj.get();
    auto it = m_ptrmap.find(key);
    if (it != m_ptrmap.end())
      return it->second;
    uint32_t idx = (uint32_t)m_objects.size();
    m_ptrmap.emplace(key, idx);
    m_objects.push_back(obj);
    return idx;
  }

  CObjectSPtr from_handle(uint32_t handle)
  {
    if (handle >= m_objects.size())
      return nullptr;
    return m_objects[handle];
  }
};

