#pragma once
#include <optional>
#include <vector>
#include <array>
#include <unordered_map>
#include <stdexcept>
#include <iterator>
#include <limits>
#include <cassert>

#include <redx/core/platform.hpp>
#include <redx/core/hashing.hpp>
#include <redx/core/utils.hpp>
#include <redx/io/bstream.hpp>
#include <redx/serialization/stringvec.hpp>

// todo: add collision detection

namespace redx {

// this is a set container for strings, it is designed for serialization.
// it does reallocate thus string_views can be invalidated on insertion.
// StringDesc must provide the same interface as stringvec_str_desc
template <class RelSpanT, bool AddNullTerminators = false>
struct stringset
{
  using str_desc = RelSpanT;
  using storage_type = stringvec<str_desc, AddNullTerminators>;
  using block_type = typename storage_type::block_type;

  stringset() = default;

  inline bool has_string(std::string_view sv) const
  {
    return m_idxmap.find(fnv1a64(sv)) != m_idxmap.end();
  }

  inline bool has_hash(const fnv1a64_t hash) const
  {
    return m_idxmap.find(hash) != m_idxmap.end();
  }

  inline void clear()
  {
    m_storage.clear();
    m_idxmap.clear();
  }

  // returns count of strings
  inline uint32_t size() const
  {
    return static_cast<uint32_t>(m_idxmap.size());
  }

  // view can be invalidated on insertion
  inline std::string_view at(size_t idx) const
  {
    return m_storage.at(idx);
  }

  inline size_t memory_size() const
  {
    return m_storage.memory_size();
  }

  inline const block_type& memory_block() const
  {
    return m_storage.memory_block();
  }

  inline block_type&& steal_memory_block()
  {
    m_idxmap.clear();
    return m_storage.steal_memory_block();
  }

  inline void shrink_block_to_fit()
  {
    m_storage.shrink_block_to_fit();
  }

  // returns pair (fnv1a64_hash, index)
  std::pair<fnv1a64_t, uint32_t> 
  inline register_string(std::string_view sv)
  {
    const fnv1a64_t hash = fnv1a64(sv);
    return register_string(sv, hash);
  }

  // returns pair (fnv1a64_hash, index)
  // the hash is not verified, use carefully
  std::pair<fnv1a64_t, uint32_t> 
  register_string(std::string_view sv, const fnv1a64_t hash)
  {
    // todo: in debug, check that the given hash is correct

    auto it = m_idxmap.find(hash);
    if (it == m_idxmap.end())
    {
      const uint32_t idx = static_cast<uint32_t>(m_storage.size());
      m_storage.push_back(sv);
      it = m_idxmap.emplace(hash, idx).first;
    }

    return *it;
  }

  inline std::optional<uint32_t> find(std::string_view sv) const
  {
    const uint64_t hash = fnv1a64(sv);
    return find(hash);
  }

  inline std::optional<uint32_t> find(const uint64_t fnv1a64_hash) const
  {
    auto it = m_idxmap.find(fnv1a64_hash);
    if (it != m_idxmap.end())
    {
      return { it->second };
    }
    return std::nullopt;
  }

  // maybe enable_if AddNullTerminators==true ?
  template<bool U = AddNullTerminators, typename Enable = std::enable_if_t<U>>
  inline void assign_block(std::vector<char>&& block)
  {
    m_storage.assign_block(std::move(block));
    rebuild_idxmap();
  }

  // matches cyberpunk serialization
  inline void serialize_in(ibstream& st, uint32_t base_offset, uint32_t descs_size, uint32_t data_size)
  {
    m_storage.serialize_in(st, base_offset, descs_size, data_size);
    rebuild_idxmap();
  }

  inline void serialize_out(obstream& st, uint32_t base_offset, uint32_t& out_descs_size, uint32_t& out_data_size)
  {
    m_storage.serialize_out(st, base_offset, out_descs_size, out_data_size);
  }

private:

  void rebuild_idxmap()
  {
    m_idxmap.clear();
    for (uint32_t i = 0; i < m_storage.size(); ++i)
    {
      uint64_t hash = fnv1a64(at(i));
      m_idxmap.emplace(hash, i);
    }
  }

  storage_type m_storage;

  // lookup

  struct identity_op {
    size_t operator()(fnv1a64_t key) const { return key; }
  };

  // key: fnv1a64_hash
  std::unordered_map<fnv1a64_t, uint32_t, identity_op> m_idxmap;
};

} // namespace redx

