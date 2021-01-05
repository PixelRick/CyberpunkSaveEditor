#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <csav/serializers.hpp>
#include <fmt/format.h>
#include <utils.hpp>

class CEnumList
{
public:
  using enum_members_t = std::vector<std::string>;
  using enum_members_sptr = std::shared_ptr<enum_members_t>;

private:
  std::unordered_map<std::string, enum_members_sptr> s_list;

  // filtered lists

  CEnumList();
  ~CEnumList() = default;

public:
  static CEnumList& get()
  {
    static CEnumList s = {};
    return s;
  }

  CEnumList(const CEnumList&) = delete;
  CEnumList& operator=(const CEnumList&) = delete;

public:
  bool is_registered(std::string_view enum_name) const
  {
    return s_list.find(std::string(enum_name)) != s_list.end();
  }

  enum_members_sptr get_enum(std::string enum_name) const
  {
    auto it = s_list.find(enum_name);
    if (it != s_list.end())
      return it->second;
    return nullptr;
  }
};

