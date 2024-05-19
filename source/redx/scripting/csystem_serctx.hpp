#pragma once
#include <fstream>
#include <memory>
#include <spdlog/spdlog.h>
#include <vector>

#include "fwd.hpp"
#include <redx/serialization/cnames_blob.hpp>
#include <redx/serialization/resids_blob.hpp>

class CSystemSerCtx {
  friend class CSystem;
  friend class archive_test;

protected:
  std::vector<CObjectSPtr> m_objects;
  std::unordered_map<uintptr_t, uint32_t> m_ptrmap;
  std::ofstream m_logfile;
  redx::csav::version m_version = {};
  bool m_is_bp_compatible = false;

public:
  redx::cnames_blob strpool;
  redx::resids_blob respool;

  CSystemSerCtx() {
    // m_logfile.open(fmt::format("CSystemSerLOG_{:08X}.txt",
    // (uint32_t)(uint64_t)this));
  }

  ~CSystemSerCtx() {
    // m_logfile.close();
  }

  void log(std::string_view s) {
    // m_logfile << s << std::endl;
  }

  void set_version(const redx::csav::version& version) {
    m_is_bp_compatible = version.v2 == 2120;
    m_version = version; 
  }

  bool is_blueprint_compatible() const {
    return m_is_bp_compatible;
  }

public:
  void rebuild_handlemap() {
    m_ptrmap.clear();
    for (size_t i = 0; i < m_objects.size(); ++i) {
      const uintptr_t key = (uintptr_t)m_objects[i].get();
      m_ptrmap.emplace(key, (uint32_t)i);
    }
  }

  uint32_t to_handle(const CObjectSPtr &obj) {
    const uintptr_t key = (uintptr_t)obj.get();
    auto it = m_ptrmap.find(key);
    if (it != m_ptrmap.end())
      return it->second;
    uint32_t idx = (uint32_t)m_objects.size();
    m_ptrmap.emplace(key, idx);
    m_objects.push_back(obj);
    return idx;
  }

  CObjectSPtr from_handle(uint32_t handle) {
    if (handle >= m_objects.size())
      return nullptr;
    return m_objects[handle];
  }
};
