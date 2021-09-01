#pragma once
#include <inttypes.h>
#include <stdlib.h>
#include <optional>
#include <vector>
#include <array>
#include <unordered_map>

#include <redx/core/cname.hpp>
#include <redx/core/hashing.hpp>
#include <redx/core/utils.hpp>
#include <redx/io/bstream.hpp> 
#include <redx/containers/bitfield.hpp>

namespace redx {

// todo: check for collisions

struct cnameset
{
protected:

  struct ser_desc
  {
    union
    {
      redx::bfm32<uint32_t, 0, 24> offset;
      redx::bfm32<uint8_t, 24, 8> size;
    };
  };

public:

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

  template <bool WithNullTerminators = true>
  bool read_in(const char* buffer, uint32_t descs_offset, uint32_t descs_size, uint32_t data_size)
  {
    clear();

    const size_t descs_cnt = descs_size / sizeof(ser_desc);
    if (descs_size % sizeof(ser_desc))
    {
      SPDLOG_ERROR("invalid given descs_size");
      return false;
    }

    const char* pdescs = buffer + descs_offset;
    const char* data = pdescs + descs_size;
    const char* data_end = data + data_size;
    std::span<const ser_desc> descs(reinterpret_cast<const ser_desc*>(pdescs), descs_cnt);
    
    for (uint32_t i = 0; i < descs.size(); ++i)
    {
      const auto& desc = descs[i];

      const uint32_t str_size = WithNullTerminators ? desc.size() - 1 : desc.size();
      std::string_view str(buffer + desc.offset(), str_size);

      if (&*str.end() > data_end)
      {
        SPDLOG_ERROR("string out of bounds");
        return false;
      }

      cname cn(str);
      m_ids.emplace_back(cn);
      m_idxmap.emplace(cn, i);
    }

    return true;
  }
  
  // todo: remove descs_pos arg (use tell)

  template <bool WithNullTerminators = true>
  void serialize_out(obstream& st, uint32_t base_spos, uint32_t& out_descs_size, uint32_t& out_data_size)
  {
    out_descs_size = 0;
    out_data_size = 0;

    std::vector<ser_desc> descs(size());

    const uint32_t descs_spos = (uint32_t)st.tellp();
    st.write_array(descs);

    const uint32_t data_spos = (uint32_t)st.tellp();
    uint32_t offset = (uint32_t)data_spos - base_spos;

    for (uint32_t i = 0; i < descs.size(); ++i)
    {
      gname gn = m_ids[i].gstr();
      if (!gn)
      {
        STREAM_LOG_AND_SET_ERROR(st, "a cname couldn't be resolved to string");
        return;
      }

      const std::string_view strv = gn.strv();
      const uint32_t str_size = (uint32_t)strv.size();

      auto& desc = descs[i];
      desc.offset = offset;

      st.write_bytes(strv.data(), strv.size());
      offset += str_size;

      if constexpr (WithNullTerminators)
      {
        st << '\0';
        desc.size = str_size + 1;
        offset++;
      }
      else
      {
        desc.size = str_size;
      }
    }

    const uint32_t end_spos = (uint32_t)st.tellp();

    st.seekp(descs_spos);
    st.write_array(descs);

    st.seekp(end_spos);

    out_descs_size = data_spos - descs_spos;
    out_data_size = end_spos - data_spos;
  }

protected:

  container_type m_ids;

  struct identity_op {
    size_t operator()(const cname& key) const { return key.hash; }
  };

  std::unordered_map<cname, uint32_t, identity_op> m_idxmap;
};

} // namespace redx

