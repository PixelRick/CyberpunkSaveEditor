#pragma once
#include <inttypes.h>
#include <optional>
#include <vector>
#include <array>
#include <unordered_map>

#include <cpinternals/common/hashing.hpp>
#include <cpinternals/common/utils.hpp>

// todo: add collision detection

namespace cp {

// this pool never reallocates thus string_views are valid until destruction of the pool.
// it behaves as a set (no duplicates) and is not clearable.
struct stringpool
{
  constexpr static size_t block_size = 0x40000;
  using block_type = std::array<char, block_size>;

  stringpool()
    : m_curpos(block_size)
  {
  }

  // non-copyable
  stringpool(const stringpool&) = delete;
  stringpool& operator=(const stringpool&) = delete;

  bool has_string(std::string_view sv) const
  {
    return m_idxmap.find(fnv1a64(sv)) != m_idxmap.end();
  }

  bool has_hash(const uint64_t fnv1a64_hash) const
  {
    return m_idxmap.find(fnv1a64_hash) != m_idxmap.end();
  }

  void reserve(size_t cnt)
  {
    m_views.reserve(cnt);
    m_idxmap.reserve(cnt);
  }

  uint32_t size() const
  {
    return static_cast<uint32_t>(m_views.size());
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
      auto new_sv = allocate_and_copy(sv);

      const uint32_t idx = static_cast<uint32_t>(m_views.size());
      m_views.emplace_back(new_sv);
      it = m_idxmap.emplace(fnv1a64_hash, idx).first;
    }

    return *it;
  }

  // s must have static storage duration.
  // if s is not already present no copy is done and a string_view of s is created.
  // (any good reason to not do like this ?)
  uint32_t register_literal(const char* const s)
  {
    std::string_view sv(s);
    const uint64_t hash = fnv1a64(sv);

    auto it = m_idxmap.find(hash);
    if (it != m_idxmap.end())
    {
      return it->second;
    }

    const uint32_t idx = static_cast<uint32_t>(m_views.size());
    m_views.emplace_back(sv);
    m_idxmap.emplace(hash, idx);

    return idx;
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

  std::string_view at(uint32_t idx) const
  {
    return m_views[idx];
  }

protected:

  bool allocate_block()
  {
    m_blocks.emplace_back(std::make_unique<block_type>());
    m_curpos = 0;
    return true;
  }

  // This allocates "len" bytes and a terminating null character.
  // so that calling the returned string_view's data() is equivalent but faster than c_str().
  std::string_view allocate_and_copy(std::string_view sv)
  {
    const size_t len = sv.size();

    if (m_curpos + len + 1 > block_size)
    {
      allocate_block();
    }

    char* data = m_blocks.back()->data() + m_curpos;
    std::copy(sv.begin(), sv.end(), data);

    m_curpos += len + 1;

    return std::string_view(data, len);
  }

  std::vector<std::unique_ptr<block_type>> m_blocks;
  size_t m_curpos;

  std::vector<std::string_view> m_views;

  // lookup

  struct identity_op {
    size_t operator()(uint64_t key) const { return key; }
  };

  // key: fnv1a64_hash
  std::unordered_map<uint64_t, uint32_t, identity_op> m_idxmap;
};

} // namespace cp

