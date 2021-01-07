#pragma once
#include <vector>
#include <memory>
#include <csav/csystem/fwd.hpp>
#include <csav/csystem/CStringPool.hpp>

class CSystemSerCtx
{
  friend class CSystem;

protected:
  std::vector<CObjectSPtr> m_objects;
  std::unordered_map<uintptr_t, uint32_t> m_ptrmap;

public:
  CStringPool strpool;

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

