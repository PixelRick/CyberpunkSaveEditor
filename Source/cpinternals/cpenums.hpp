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

class CEnumList
{
public:
  using enum_members_t = std::vector<std::string>;
  using enum_members_sptr = std::shared_ptr<enum_members_t>;

private:
  std::unordered_map<std::string, enum_members_sptr> m_enummap;

  // filtered lists

  CEnumList();
  ~CEnumList() = default;

public:
  CEnumList(const CEnumList&) = delete;
  CEnumList& operator=(const CEnumList&) = delete;

  static CEnumList& get()
  {
    static CEnumList s = {};
    return s;
  }

public:
  bool is_registered(std::string_view enum_name) const
  {
    return m_enummap.find(std::string(enum_name)) != m_enummap.end();
  }

  enum_members_sptr get_enum(std::string enum_name) const
  {
    auto it = m_enummap.find(enum_name);
    if (it != m_enummap.end())
      return it->second;
    return nullptr;
  }
};

