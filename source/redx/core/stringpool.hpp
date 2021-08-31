#pragma once
#include <redx/core/platform.hpp>

#include <optional>
#include <vector>
#include <array>
#include <unordered_map>
#include <shared_mutex>
#include <type_traits>
#include <limits>
#include <memory>

#include <redx/core/hashing.hpp>
#include <redx/core/utils.hpp>

// this pool never reallocates thus string_views are valid until destruction of the pool.
// it behaves as a set (no duplicates) and is not clearable.

namespace redx {

// a string_view can be built from the string location directly
// that's better than storing views elsewhere (better locality)
struct stringpool_entry
{
  static constexpr size_t stride = 4;

  static constexpr size_t get_needed_size(size_t src_len)
  {
    return offsetof(stringpool_entry, buf) + src_len + 1;
  }

  stringpool_entry(std::string_view sv)
  {
    size = static_cast<uint16_t>(sv.size());
    std::copy(sv.data(), sv.data() + size, buf);
    buf[size] = '\0';
  }

  FORCE_INLINE std::string_view strv() const noexcept
  {
    return {buf, size};
  }

private:

  uint16_t size;
  char buf[1];
};

static_assert(stringpool_entry::stride >= alignof(stringpool_entry));

// supports different implementations of index_type
// to tweak performance

template <size_t BlockSize, size_t BlockCnt>
struct stringpool_storage_base
{
  static constexpr size_t block_size = BlockSize;
  static constexpr size_t block_cnt = BlockCnt;

  struct block_type
  {
    inline const char* data() const noexcept { return _data; }
    inline       char* data()       noexcept { return _data; }

    alignas(stringpool_entry::stride) char _data[block_size];
  };

  stringpool_storage_base() noexcept = default;

  // non-copyable
  stringpool_storage_base(const stringpool_storage_base&) = delete;
  stringpool_storage_base& operator=(const stringpool_storage_base&) = delete;

  stringpool_entry* allocate_entry(const std::string_view& sv, uint32_t& out_block_idx, size_t& out_entry_idx)
  {
    stringpool_entry* ret = try_alloc_entry_in_current_block(sv, out_block_idx, out_entry_idx);
    if (!ret)
    {
      if (m_blocks[m_cur_block_index]) // for first block allocation
      {
        m_cur_block_index++;
      }

      if (m_cur_block_index >= block_cnt)
      {
        SPDLOG_CRITICAL("out of blocks");
        DEBUG_BREAK();
      }

      m_blocks[m_cur_block_index] = std::make_unique<block_type>();
      m_cur_block_space = block_size;

      ret = try_alloc_entry_in_current_block(sv, out_block_idx, out_entry_idx);
      if (!ret)
      {
        SPDLOG_CRITICAL("block too small for source");
        DEBUG_BREAK();
      }
    }

    return ret;
  }

  inline const stringpool_entry* at(uint32_t block_idx, uint32_t entry_idx) const
  {
    const size_t offset = block_offset_from_entry_idx(entry_idx);

    if (block_idx < block_cnt && offset < block_size)
    {
      const char* block = m_blocks[block_idx]->data();
      if (block)
      {
        return reinterpret_cast<stringpool_entry>(block + offset);
      }
    }

    return nullptr;
  }

  inline const stringpool_entry* at_unsafe(uint32_t block_idx, uint32_t entry_idx) const
  {
    const size_t offset = block_offset_from_entry_idx(entry_idx);
    return reinterpret_cast<const stringpool_entry*>(m_blocks[block_idx]->data() + offset);
  }

private:

  static inline constexpr size_t block_offset_from_entry_idx(size_t entry_idx) noexcept
  {
    return entry_idx * stringpool_entry::stride;
  }

  stringpool_entry* try_alloc_entry_in_current_block(const std::string_view& sv, uint32_t& out_block_idx, size_t& out_entry_idx)
  {
    const size_t needed_size = stringpool_entry::get_needed_size(sv.size());

    char* const beg = m_blocks[m_cur_block_index]->data();
    void* start = beg + block_size - m_cur_block_space;

    void* alloc = std::align(stringpool_entry::stride, needed_size, start, m_cur_block_space);
    if (!alloc)
    {
      return nullptr;
    }

    m_cur_block_space -= needed_size;
    out_block_idx = m_cur_block_index;
    out_entry_idx = ((char*)alloc - beg) / stringpool_entry::stride;

    // construct
    return new (alloc) stringpool_entry(sv);
  }

  std::array<std::unique_ptr<block_type>, 0x1000> m_blocks;
  uint32_t  m_cur_block_index = 0;
  size_t    m_cur_block_space = 0;
  size_t    m_entries_cnt = 0;
};


enum class stringpool_variant
{
  default_u32_indices,
  fast_u64_indices,
};

// this default size should not exceed the maximum block size of the default variant
inline constexpr size_t stringpool_default_block_size = 0x10000 * stringpool_entry::stride;
inline constexpr size_t stringpool_default_block_cnt = 0x2000;

template <stringpool_variant Variant, size_t BlockSize = stringpool_default_block_size, size_t BlockCnt = stringpool_default_block_cnt>
struct stringpool_storage
{
  static_assert(!std::is_same_v<Variant, Variant>, "stringpool_storage has no defined specialization for this Variant");
};

template <size_t BlockSize, size_t BlockCnt>
struct stringpool_storage<stringpool_variant::default_u32_indices, BlockSize, BlockCnt>
  : protected stringpool_storage_base<BlockSize, BlockCnt>
{
  using base = stringpool_storage_base<BlockSize, BlockCnt>;

  static constexpr size_t max_block_size = 0x10000 * stringpool_entry::stride;
  static_assert(BlockSize <= max_block_size, "BlockSize is too big for this variant");

  struct index_type
  {
    static constexpr uint16_t null_block_idx = std::numeric_limits<uint16_t>::max();

    index_type() noexcept = default;

    friend FORCE_INLINE bool operator<(const index_type& a, const index_type& b)
    {
      return a.block_idx < b.block_idx || a.entry_idx < b.entry_idx;
    }

    friend FORCE_INLINE bool operator==(const index_type& a, const index_type& b)
    {
      return a.block_idx == b.block_idx && a.entry_idx == b.entry_idx;
    }

    friend FORCE_INLINE bool operator!=(const index_type& a, const index_type& b)
    {
      return !(a == b);
    }

    inline bool is_null() const noexcept
    {
      return block_idx == null_block_idx;
    }

    inline size_t hash() const noexcept
    {
      return std::hash<uint32_t>()(static_cast<uint32_t>(entry_idx) << 16 | block_idx);
    }

  private:
    
    friend stringpool_storage;

    index_type(uint16_t block_idx, uint16_t entry_idx)
      : block_idx(block_idx), entry_idx(entry_idx) {}

    uint16_t block_idx = null_block_idx;
    uint16_t entry_idx; // offset = entry_idx * entry::stride
  };

  using base::base;

  index_type allocate_entry(std::string_view sv)
  {
    uint32_t block_idx{};
    size_t entry_idx{};
    base::allocate_entry(sv, block_idx, entry_idx);
    return index_type(static_cast<uint16_t>(block_idx), static_cast<uint16_t>(entry_idx));
  }

  inline std::string_view at(const index_type& idx) const noexcept
  {
    return idx.is_null() ? "" : base::at_unsafe(idx.block_idx, idx.entry_idx)->strv();
  }
};

template <size_t BlockSize, size_t BlockCnt>
struct stringpool_storage<stringpool_variant::fast_u64_indices, BlockSize, BlockCnt>
  : protected stringpool_storage_base<BlockSize, BlockCnt>
{
  using base = stringpool_storage_base<BlockSize, BlockCnt>;

  struct index_type
  {
    index_type() noexcept = default;

    friend FORCE_INLINE bool operator<(const index_type& a, const index_type& b)
    {
      return a.entry < b.entry;
    }

    friend FORCE_INLINE bool operator==(const index_type& a, const index_type& b)
    {
      return a.entry == b.entry;
    }

    friend FORCE_INLINE bool operator!=(const index_type& a, const index_type& b)
    {
      return !(a == b);
    }

    FORCE_INLINE bool is_null() const noexcept
    {
      return !entry;
    }

    FORCE_INLINE size_t hash() const noexcept
    {
      return std::hash<uintptr_t>()((uintptr_t)entry);
    }

  private:
    
    friend stringpool_storage;

    index_type(stringpool_entry* entry)
      : entry(entry) {}

    stringpool_entry* entry = nullptr;
  };

  using base::base;

  index_type allocate_entry(std::string_view sv)
  {
    uint32_t block_idx{};
    size_t entry_idx{};
    return index_type(base::allocate_entry(sv, block_idx, entry_idx));
  }

  inline std::string_view at(const index_type& idx) const noexcept
  {
    return idx.is_null() ? "" : idx.entry->strv();
  }
};

// index_type requires specialization
template <
  bool ThreadSafe,
  stringpool_variant Variant = stringpool_variant::default_u32_indices,
  size_t BlockSize = stringpool_default_block_size,
  size_t BlockCnt = stringpool_default_block_cnt>
struct stringpool
{
  using mutex_type = std::conditional_t<ThreadSafe, std::shared_mutex, redx::nop_mutex>;

  using storage_type = stringpool_storage<Variant, BlockSize, BlockCnt>;
  using index_type = typename storage_type::index_type;

  stringpool() = default;

  // non-copyable
  stringpool(const stringpool&) = delete;
  stringpool& operator=(const stringpool&) = delete;

  bool has_string(std::string_view sv) const
  {
    std::shared_lock<mutex_type> sl(m_smtx);
    return m_idxmap.find(fnv1a64(sv)) != m_idxmap.end();
  }

  bool has_hash(const fnv1a64_t hash) const
  {
    std::shared_lock<mutex_type> sl(m_smtx);
    return m_idxmap.find(hash) != m_idxmap.end();
  }

  void reserve(size_t cnt)
  {
    std::unique_lock<mutex_type> ul(m_smtx);
    m_idxmap.reserve(cnt);
  }

  size_t size() const
  {
    std::shared_lock<mutex_type> sl(m_smtx);
    return m_idxmap.size();
  }

  // returns pair (fnv1a64_hash, index)
  std::pair<fnv1a64_t, index_type> 
  register_string(std::string_view sv)
  {
    const fnv1a64_t hash = fnv1a64(sv);
    return register_string(sv, hash);
  }

  // returns pair (fnv1a64_hash, index)
  // the hash is not verified, use carefully
  std::pair<fnv1a64_t, index_type> 
  register_string(std::string_view sv, const fnv1a64_t hash)
  {
    // todo: in debug, check that the given hash is correct

    std::shared_lock<mutex_type> sl(m_smtx, std::defer_lock);
    std::unique_lock<mutex_type> ul(m_smtx, std::defer_lock);

    bool found = false;
    std::pair<fnv1a64_t, index_type> ret = {};

    sl.lock();
    auto it = m_idxmap.find(hash);
    found = (it != m_idxmap.end());

    if (!found)
    {
      // switch from shared to unique lock
      sl.unlock();
      ul.lock();

      // another registration could have gotten the write lock before us
      // let's check again for an entry
      if constexpr (ThreadSafe)
      {
        it = m_idxmap.find(hash);
        found = (it != m_idxmap.end());
      }
      
      if (!found)
      {
        index_type idx = m_storage.allocate_entry(sv);

        ret = {hash, std::move(idx)};
        m_idxmap.emplace(ret);
      }

      ul.unlock();
    }
    else
    {
      ret = *it;
      sl.unlock();
    }

    if (found && m_collision_check_enabled)
    {
      auto existing_sv = at(ret.second);
      if (existing_sv != sv)
      {
        SPDLOG_ERROR("string hash collision: \"{}\" vs \"{}\"", existing_sv, sv);
      }
    }

    return ret;
  }

  FORCE_INLINE std::optional<index_type> find(std::string_view sv) const
  {
    const fnv1a64_t hash = fnv1a64(sv);
    return find(hash);
  }

  std::optional<index_type> find(const fnv1a64_t hash) const
  {
    std::shared_lock<mutex_type> sl(m_smtx);
    auto it = m_idxmap.find(hash);
    if (it != m_idxmap.end())
    {
      return { it->second };
    }
    return std::nullopt;
  }

  inline std::string_view at(index_type idx) const
  {
    std::shared_lock<mutex_type> sl(m_smtx);
    return m_storage.at(idx);
  }

private:

  bool m_collision_check_enabled = false;

  mutable mutex_type m_smtx;
  storage_type m_storage;

  // lookup

  struct identity_op {
    size_t operator()(fnv1a64_t key) const { return key; }
  };

  std::unordered_map<fnv1a64_t, index_type, identity_op> m_idxmap;
};

template <
  stringpool_variant Variant = stringpool_variant::default_u32_indices,
  size_t BlockSize = stringpool_default_block_size,
  size_t BlockCnt = stringpool_default_block_cnt>
using stringpool_st = stringpool<false, Variant, BlockSize, BlockCnt>;

template <
  stringpool_variant Variant = stringpool_variant::default_u32_indices,
  size_t BlockSize = stringpool_default_block_size,
  size_t BlockCnt = stringpool_default_block_cnt>
using stringpool_mt = stringpool<true, Variant, BlockSize, BlockCnt>;

} // namespace redx

