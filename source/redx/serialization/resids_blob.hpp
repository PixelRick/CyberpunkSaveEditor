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
#include <redx/containers/bitfield.hpp>
#include <redx/io/bstream.hpp>

namespace redx {

// temp
struct resource_id
{
  cname path;
  bool flag = false;

  resource_id() = default;
  resource_id(cname path, bool flag)
    : path(path), flag(flag) {}

  bool operator==(const resource_id& rhs) const noexcept
  {
    return path == rhs.path && flag == rhs.flag;
  }

  bool operator<(const resource_id& rhs) const noexcept
  {
    return path < rhs.path || flag < rhs.flag;
  }
};

struct resids_blob
{
protected:

  struct ser_desc
  {
    union
    {
      redx::bfm32<uint32_t, 0, 23> offset;
      redx::bfm32<uint8_t, 23, 8> size;
      redx::bfm32<uint8_t, 31, 1> flag;
    };
  };

public:

  using container_type = std::vector<resource_id>;

  resids_blob() {}

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

  bool contains(std::string_view sv) const
  {
    return contains(fnv1a64(sv));
  }

  bool contains(cname cn) const
  {
    return find(cn) != (uint32_t)-1;
  }

  bool contains(const uint64_t hash) const
  {
    return find(cname(hash)) != (uint32_t)-1;
  }

  uint32_t size() const { return static_cast<uint32_t>(m_ids.size()); }

  uint32_t insert(std::string_view sv, bool flag)
  {
    return insert(cname(sv), flag);
  }

  uint32_t insert(cname cn, bool flag)
  {
    
    uint32_t idx = find(cn);

    if (idx == (uint32_t)-1)
    {
      idx = static_cast<uint32_t>(m_ids.size());
      m_ids.emplace_back(cn, flag);
      m_idxmap.emplace(cn, idx);
    }

    m_ids[idx].flag |= flag; // flag 1 wins on dupe paths (see hanako.app's buffer 0)

    return idx;
  }

  uint32_t find(const cname& path) const
  {
    auto it = m_idxmap.find(path);
    if (it != m_idxmap.end())
    {
      return it->second;
    }

    return (uint32_t)-1;
  }

  const resource_id& at(uint32_t idx) const
  {
    return m_ids[idx];
  }

  bool read_in(const char* buffer, uint32_t base_offset, uint32_t descs_size, uint32_t data_size, bool as_hash)
  {
    clear();

    const size_t descs_cnt = descs_size / sizeof(ser_desc);
    if (descs_size % sizeof(ser_desc))
    {
      SPDLOG_ERROR("invalid given descs_size");
      return false;
    }

    const char* base = buffer + base_offset;
    const char* data = base + descs_size;
    const char* data_end = data + data_size;
    std::span<const ser_desc> descs(reinterpret_cast<const ser_desc*>(base), descs_cnt);

    for (uint32_t i = 0; i < descs.size(); ++i)
    {
      const auto& desc = descs[i];
      cname cn;

      if (as_hash)
      {
        uint64_t* phash = (uint64_t*)(buffer + desc.offset());
        if (phash + 1 > (uint64_t*)data_end)
        {
          SPDLOG_ERROR("hash out of bounds");
          return false;
        }
        cn = cname(*phash);
      }
      else
      {
        std::string_view str(buffer + desc.offset(), desc.size());
        if (&*str.end() > data_end)
        {
          SPDLOG_ERROR("string out of bounds");
          return false;
        }
        cn = str;
      }

      m_ids.emplace_back(cn, desc.flag);
      m_idxmap.emplace(cn, i);
    }

    return true;
  }
  
  // todo: remove descs_pos arg (use tell)

  template <bool AddNullTerminators = false>
  void serialize_out(obstream& st, uint32_t base_spos, uint32_t& out_descs_size, uint32_t& out_data_size, bool as_hash)
  {
    out_descs_size = 0;
    out_data_size = 0;

    std::vector<ser_desc> descs(size());

    const uint32_t descs_spos = (uint32_t)st.tellp();
    st.write_array(descs.data(), descs.size()); // dummy

    const uint32_t data_spos = (uint32_t)st.tellp();
    uint32_t offset = data_spos - base_spos;

    for (uint32_t i = 0; i < descs.size(); ++i)
    {
      const auto& rid = m_ids[i];

      if (as_hash)
      {
        auto& desc = descs[i];
        desc.offset = offset;
        desc.size = 8;
        desc.flag = rid.flag;

        st << rid.path.hash;
        offset += 8;
      }
      else
      {
        gname gn = rid.path.gstr();
        if (!gn)
        {
          STREAM_LOG_AND_SET_ERROR(st, "a res path couldn't be resolved to string");
          return;
        }

        const std::string_view strv = gn.strv();
        const uint32_t str_size = (uint32_t)strv.size();

        auto& desc = descs[i];
        desc.offset = offset;
        desc.size = (uint32_t)str_size;
        desc.flag = rid.flag;

        st.write_bytes(strv.data(), strv.size());
        offset += str_size;

        if constexpr (AddNullTerminators)
        {
          st.write_byte('\0');
          offset++;
        }
      }
    }

    const uint32_t end_spos = (uint32_t)st.tellp();

    st.seekp(descs_spos);
    st.write_array(descs.data(), descs.size());

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

