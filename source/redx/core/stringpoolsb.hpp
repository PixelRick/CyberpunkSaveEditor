#pragma once
#include <inttypes.h>
#include <optional>
#include <vector>
#include <array>
#include <unordered_map>
#include <stdexcept>
#include <iterator>

#include <redx/common/hashing.hpp>
#include <redx/common/utils.hpp>
#include <redx/common/streambase.hpp>
#include <redx/containers/bitfield.hpp>

// todo: add collision detection

namespace redx {

// this is the single block variant of stringpool, it is designed for serialization.
// this pool does reallocate thus string_views can be invalidated on insert.
// descriptors (offset, size) are encoded in 4 bytes

template <bool AddNullTerminators = false>
struct stringpoolsb
{
  using block_type = std::vector<char>;

  struct str_desc
  {
    union
    {
      redx::bfm32<uint32_t, 0, 24> offset;
      redx::bfm32<uint8_t, 24, 8> size;
    };
  };

  stringpoolsb() = default;

  bool has_string(std::string_view sv) const
  {
    return m_idxmap.find(fnv1a64(sv)) != m_idxmap.end();
  }

  bool has_hash(const uint64_t fnv1a64_hash) const
  {
    return m_idxmap.find(fnv1a64_hash) != m_idxmap.end();
  }

  uint32_t size() const
  {
    return static_cast<uint32_t>(m_descs.size());
  }

  void clear()
  {
    m_descs.clear();
    m_idxmap.clear();
    m_block.clear();
  }

  size_t memory_size() const
  {
    return m_block.size();
  }

  void shrink_block_to_fit()
  {
    m_block.shrink_to_fit();
  }

  // returns pair (fnv1a64_hash, index)
  std::pair<uint64_t, uint32_t> 
  register_string(std::string_view sv)
  {
    const uint64_t hash = fnv1a64(sv);
    return register_string(sv, hash);
  }

  // returns pair (fnv1a64_hash, index)
  // the hash is not verified, use carefully
  std::pair<uint64_t, uint32_t> 
  register_string(std::string_view sv, const uint64_t fnv1a64_hash)
  {
    // todo: in debug, check that the given hash is correct

    auto it = m_idxmap.find(fnv1a64_hash);
    if (it == m_idxmap.end())
    {
      auto new_desc = allocate_and_copy(sv);

      const uint32_t idx = static_cast<uint32_t>(m_descs.size());
      m_descs.emplace_back(new_desc);
      it = m_idxmap.emplace(fnv1a64_hash, idx).first;
    }

    return *it;
  }

  std::optional<uint32_t> find(std::string_view sv) const
  {
    const uint64_t hash = fnv1a64(sv);
    return find(hash);
  }

  std::optional<uint32_t> find(const uint64_t fnv1a64_hash) const
  {
    auto it = m_idxmap.find(fnv1a64_hash);
    if (it != m_idxmap.end())
    {
      return { it->second };
    }
    return std::nullopt;
  }

  // view can be invalidated on pool insertion (register)
  std::string_view at(uint32_t idx) const
  {
    const auto& desc = m_descs[idx];
    return std::string_view(m_block.data() + desc.offset(), desc.size());
  }

  friend streambase& operator<<(streambase& sb, stringpoolsb& x)
  {
    if (sb.is_reader())
    {
      x.serialize_in(sb);
    }
    else
    {
      x.serialize_out(sb);
    }

    return sb;
  }

  // matches cyberpunk serialization
  void serialize_in(streambase& sb, uint32_t base_offset, uint32_t descs_size, uint32_t data_size)
  {
    if (!sb.is_reader())
    {
      sb.set_error("stringpoolsb::serialize_in: cannot be used with output stream");
      return;
    }

    clear();

    if (descs_size % sizeof(str_desc) != 0)
    {
      sb.set_error("stringpoolsb::serialize_in: invalid given descs_size");
      return;
    }

    const size_t descs_cnt = descs_size / sizeof(str_desc);
    if (descs_cnt == 0)
    {
      if (data_size != 0)
      {
        sb.set_error("stringpoolsb::serialize_in: descs_cnt == 0 but data_size != 0");
      }
      return;
    }

    m_descs.resize(descs_cnt);
    sb.serialize_pods_array_raw(m_descs.data(), descs_cnt);

    // find end
    uint32_t max_offset = 0;
    for (auto& desc : m_descs)
    {
      const uint32_t end_offset = desc.end_offset();
      if (end_offset > max_offset)
        max_offset = end_offset;
    }

    // iterator is valid
    const uint32_t real_end_offset = base_offset + descs_size + data_size;
    if (max_offset > real_end_offset)
    {
      sb.set_error("stringpoolsb::serialize_in: max_offset > real_end_offset");
      return;
    }

    m_block.resize(data_size);
    sb.serialize_bytes(m_block.data(), data_size);

    // fix desc offsets and compute idxmap
    uint32_t idx = 0;
    for (auto& desc : m_descs)
    {
      const uint32_t offset = desc.offset();

      if (offset < base_offset + descs_size)
      {
        sb.set_error("stringpoolsb::serialize_in: desc offset is lower than expected");
        clear(); // prevent invalid state
        return;
      }

      desc.set_offset(offset - base_offset - descs_size);

      if (desc.end_offset() > data_size)
      {
        sb.set_error("stringpoolsb::serialize_in: desc end offset is bigger than expected");
        clear(); // prevent invalid state
        return;
      }

      // at(idx) is safe to use now
      uint64_t hash = fnv1a64(at(idx));
      m_idxmap.emplace(hash, idx++);
    }
  }

  void serialize_out(streambase& sb, uint32_t base_offset, uint32_t& out_descs_size, uint32_t& out_data_size)
  {
    // todo: the additional offseting here can make current descriptors offsets overflow..
    //       so it would be nice to detect it early

    out_descs_size = 0;
    out_data_size = 0;

    if (sb.is_reader())
    {
      sb.set_error("stringpoolsb::serialize_out: cannot be used with input stream");
      return;
    }

    const uint32_t desc_cnt = (uint32_t)m_descs.size();
    const uint32_t descs_size = (uint32_t)(desc_cnt * sizeof(str_desc));
    const uint32_t data_offset = base_offset + descs_size;
    const uint32_t data_size = (uint32_t)m_block.size();

    std::vector<str_desc> new_descs = m_descs;

    for (auto& desc : new_descs)
    {
      desc.set_offset(desc.offset() + data_offset);
    }

    sb.serialize_pods_array_raw(new_descs.data(), desc_cnt);
    sb.serialize_bytes(m_block.data(), data_size);

    out_descs_size = descs_size;
    out_data_size = data_size;
  }

protected:

  // This allocates "len" bytes and a terminating null character if AddNullTerminators is true.
  str_desc allocate_and_copy(std::string_view sv)
  {
    const size_t offset = m_block.size();
    if (offset > str_desc::max_offset)
    {
      throw std::exception("stringpoolsb::allocate: current offset cannot fit in descriptor");
    }

    // let's use geometric growth, and provide a shrink_to_fit
    std::copy(sv.begin(), sv.end(), std::back_inserter(m_block));

    if constexpr (AddNullTerminators)
    {
      m_block.push_back('\0');
    }

    return str_desc(offset, sv.size());
  }

  block_type m_block;

  std::vector<str_desc> m_descs;

  // lookup

  struct identity_op {
    size_t operator()(uint64_t key) const { return key; }
  };

  // key: fnv1a64_hash
  std::unordered_map<uint64_t, uint32_t, identity_op> m_idxmap;
};

} // namespace redx

