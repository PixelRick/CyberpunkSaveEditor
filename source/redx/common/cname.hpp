#pragma once
#include <inttypes.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <redx/common/streambase.hpp>
#include <redx/common/gstrid.hpp>
#include <redx/common/gname.hpp>

namespace redx {

//--------------------------------------------------------
//  cname
// 
// name with average constant access to value (lookup).
struct cname
  : public gstrid<gname::pool_tag>
{
  using base_type = gstrid<gname::pool_tag>;

  using base_type::base_type;
  using base_type::operator=;

  friend streambase& operator<<(streambase& ar, cname& cn)
  {
    if (ar.flags() & armanip::cnamehash)
    {
      return ar << cn.hash;
    }
  
    if (ar.is_reader())
    {
      std::string s;
      ar << s;
      cn = cname(s);
    }
    else
    {
      auto gs = cn.gstr();
      if (!gs)
      {
        // todo:
        // debug break instead ?
        // just set error ?
        throw std::runtime_error(fmt::format("trying to serialize <cname:{:016X}> as string but the string is unknown", cn.hash));
      }
      std::string s = gs.string();
      ar << s;
    }
  
    return ar;
  }

  std::string string() const
  {
    auto gn = gstr();

    if (!gn)
    {
      return fmt::format("<cname:{:016X}>", hash);
    }

    return gn.string();
  }
};

static_assert(sizeof(cname) == 8);

//--------------------------------------------------------
//  database: sorted names
//  mainly for imgui combo boxes..
//  can be removed when the combo box is reworked properly

struct cname_db
{
  cname_db(const cname_db&) = delete;
  cname_db& operator=(const cname_db&) = delete;

  static cname_db& get()
  {
    static cname_db s = {};
    return s;
  }

  bool is_registered(std::string_view name) const
  {
    return is_registered(cname(name));
  }

  bool is_registered(const cname& cid) const
  {
    return is_registered(cid.hash);
  }

  bool is_registered(uint64_t hash) const
  {
    return gname::find(hash).has_value();
  }

  //bool is_registered_32(uint32_t hash) const
  //{
  //  return m_invmap_32.find(hash) != m_invmap_32.end();
  //}

  void register_str(gname name)
  {
    auto p = insert_sorted_nodupe(m_full_list, name);
    if (!p.second)
      return;
  }

  void register_str(std::string_view name)
  {
    register_str(gname(name));
  }

  //gname resolve_32(uint32_t hash32) const
  //{
  //  auto it = m_invmap_32.find(hash32);
  //  if (it != m_invmap_32.end())
  //    return it->second;
  //  return gname();
  //}

  const std::vector<gname>& sorted_names() const
  {
    return m_full_list;
  }

  void feed(const std::vector<gname>& names)
  {
    m_full_list.reserve(m_full_list.size() + names.size());
    auto hint = m_full_list.begin(); // names must be pre-sorted
    for (auto& name : names)
    {
      auto p = insert_sorted_nodupe(m_full_list, hint, name);
      hint = p.first;
    }
  }

protected:

  cname_db() = default;
  ~cname_db() = default;

protected:

  struct identity_op_32 {
    size_t operator()(uint32_t key) const { return key; }
  };

  std::vector<gname> m_full_list;
  //std::unordered_map<uint32_t, gname, identity_op_32> m_invmap_32;
};

} // namespace redx

namespace std {

template<> struct hash<redx::cname>
{
    std::size_t operator()(const redx::cname& x) const noexcept
    {
        return x.hash;
    }
};

} // namespace std

