#pragma once
#include <inttypes.h>
#include <optional>
#include <vector>
#include <array>
#include <unordered_map>

#include <redx/core/cname.hpp>
#include <redx/core/hashing.hpp>
#include <redx/core/utils.hpp>
#include <redx/io/bstream.hpp>
#include <redx/serialization/stringset.hpp>

namespace redx {

// todo: check for collisions

struct cnameset
{
  using container_type = std::vector<cname>;

  cnameset() {}

  void clear()
  {
    m_ids.clear();
    m_idxmap.clear();
  }

  container_type::const_iterator begin() const
  {
    return m_ids.begin();
  }

  container_type::const_iterator end() const
  {
    return m_ids.end();
  }

  bool has_name(std::string_view sv) const
  {
    // let's not insert sv in the gname pool!
    const uint64_t hash = fnv1a64(sv);
    return has_hash(hash);
  }

  bool has_name(gname gn) const
  {
    return has_name(gn.strv());
  }

  bool has_name(cname cn) const
  {
    return has_hash(cn.hash);
  }

  bool has_hash(const uint64_t hash) const
  {
    return find(hash) != (uint32_t)-1;
  }

  uint32_t size() const { return (uint32_t)m_ids.size(); }

  uint32_t insert(std::string_view sv)
  {
    return insert(cname(sv));
  }

  uint32_t insert(gname gn)
  {
    return insert(gn.strv());
  }

  uint32_t insert(cname cn)
  {
    uint32_t idx = find(cn);

    if (idx == (uint32_t)-1)
    {
      idx = static_cast<uint32_t>(m_ids.size());
      m_ids.emplace_back(cn);
      m_idxmap.emplace(cn, idx);
    }

    return idx;
  }

  uint32_t find(std::string_view sv) const
  {
    // let's not insert sv in the gname pool!
    const uint64_t hash = fnv1a64(sv);
    return find(hash);
  }

  uint32_t find(gname gn) const
  {
    return find(gn.strv());
  }

  uint32_t find(cname cn) const
  {
    auto it = m_idxmap.find(cn);
    if (it != m_idxmap.end())
    {
      return it->second;
    }

    return (uint32_t)-1;
  }

  uint32_t find(uint64_t hash) const
  {
    auto gnopt = gname::find(hash);
    if (gnopt.has_value())
    {
      return find(gnopt.value());
    }

    return (uint32_t)-1;
  }

  cname at(uint32_t idx) const
  {
    return m_ids[idx];
  }

  template <size_t StringSizeBits>
  void serialize_in(ibstream& st, uint32_t base_offset, uint32_t descs_size, uint32_t data_size)
  {
    clear();

    stringvec<relspan_packed<StringSizeBits>> svec;
    svec.serialize_in(st, base_offset, descs_size, data_size);

    if (!st)
    {
      return;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < svec.size(); ++i)
    {
      cname cn(svec.at(i));
      m_ids.emplace_back(cn);
      m_idxmap.emplace(cn, idx++);
    }
  }

  template <size_t StringSizeBits>
  void serialize_out(obstream& st, uint32_t base_offset, uint32_t& out_descs_size, uint32_t& out_data_size)
  {
    out_descs_size = 0;
    out_data_size = 0;

    stringset<relspan_packed<StringSizeBits>> sset;

    for (auto& cn : m_ids)
    {
      gname gn = cn.gstr();

      if (!gn)
      {
        STREAM_LOG_AND_SET_ERROR(st, "a cname couldn't be resolved to string");
        return;
      }

      const std::string_view strv = gn.strv();
      sset.register_string(strv);
    }

    sset.serialize_out(st, base_offset, out_descs_size, out_data_size);
  }

protected:

  container_type m_ids;

  struct identity_op {
    size_t operator()(const cname& key) const { return key.hash; }
  };

  std::unordered_map<cname, uint32_t, identity_op> m_idxmap;
};

} // namespace redx

