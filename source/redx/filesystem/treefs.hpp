#pragma once
#include <filesystem>
#include <vector>
#include <shared_mutex>
#include <unordered_set>
#include <stack>

#include <redx/core.hpp>
#include <redx/archive/archive.hpp>
#include <redx/io/archive_file_istream.hpp>

// to diff depots, the file system must be instantiatable
// goals:
//  - minimize memory usage for one instance (for the dokany-based project)
//  - provide std-like features to make it easy to learn&use
//  
//

namespace redx::filesystem {

constexpr auto unidentified_files_directory_name = "unidentified_files";

struct treefs;
struct directory_entry;
struct directory_iterator;
struct recursive_directory_iterator;

// fse: file system entry

using fs_gname = gstring<'RADR'>;
using file_info = redx::archive::file_info;
using file_handle = redx::archive::file_handle;

// it is different from redx::file_istream since the source isn't the same !
// (could probably get another name..)
using file_istream = redx::archive_file_istream;

namespace detail::treefs {

enum class entry_kind : uint8_t
{
  none = 0,
  root,
  directory,
  file,
  reserved_for_file, // file not yet mounted
};

// this struct must remain relatively small because cp's depot contains ~600k dir entries
struct entry
{
  enum flag : uint8_t
  {
    none = 0,
    has_depot_path = 1,
  };

  //entry() : first_child() {}
  entry() = default;

  entry(path_id pid, fs_gname name, entry_kind kind, bool has_depot_path)
    : pid(pid), name(name), kind(kind)
    , flags(has_depot_path ? flag::has_depot_path : flag::none)
  {
  }

  inline bool is_reserved_for_file() const
  {
    return kind == entry_kind::reserved_for_file;
  }

  inline bool is_directory() const
  {
    return kind == entry_kind::directory || kind == entry_kind::root;
  }

  inline bool is_file() const
  {
    return kind == entry_kind::file;
  }

  inline bool is_regular() const
  {
    return is_file() || is_directory();
  }

  path_id     pid;

  int32_t     parent_entry_idx = -1;
  int32_t     next_entry_idx = -1;

  fs_gname    name;

  entry_kind  kind = entry_kind::none;

  char        unused1[3];

  int16_t     archive_idx = -1;
  uint8_t     override_cnt = 0; // when multiple archives contain the file
  uint8_t     flags = flag::none;

  union // saving some space
  {
    // not using entry_index here to keep the union trivial
    int32_t  first_child_entry_idx = -1;
    int32_t  file_idx;
  };
};

static_assert(sizeof(entry) <= 0x20);
static_assert(std::is_default_constructible_v<entry>);

} // namespace detail::treefs

// read-only tree file system.
// it only has basic thread-safety "1 writer xor N readers"
// (don't load archives while iterating through directories..)
// locking will come later, dunno yet if it should be done by accessors..
struct treefs
{
protected:

  friend struct directory_entry;
  friend struct directory_iterator;
  friend struct recursive_directory_iterator;

  using entry_kind  = detail::treefs::entry_kind;
  using entry       = detail::treefs::entry;
  
  static constexpr int32_t root_idx       = 0;
  static constexpr int32_t unids_idx      = 1;

public:

  treefs()
  {
    fs_gname::pool_reserve(0x20000);

    m_pidlinks.reserve(0x20000);
    m_entries.reserve(0x20000);

    auto& root = m_entries.emplace_back(path_id::root(), fs_gname(""), entry_kind::root, true);
    m_pidlinks.emplace(path_id::root(), root_idx);

    int32_t idx = insert_child_entry(
      root_idx, fs_gname(unidentified_files_directory_name), entry_kind::directory, false
    ).first;

    assert(idx == unids_idx);
  }

  ~treefs() = default;

  // load paths from custom format
  bool load_ardb(const std::filesystem::path& arpath);

  bool load_archive(const std::filesystem::path& path);

  // returns total size with files' first segment decompressed
  size_t get_total_size() const
  {
    compute_info();
    return m_total_size;
  }

  size_t get_total_compressed_size() const
  {
    compute_info();
    return m_total_compressed_size;
  }

  file_handle get_file_handle(path_id pid)
  {
    auto idx = find_entry_idx(pid);
    if (idx >= 0)
    {
      const auto& e = m_entries[idx];
      if (e.is_file())
      {
        return m_archives[e.archive_idx]->get_file_handle(e.file_idx);
      }
    }
    return file_handle();
  }

  std::optional<path> get_depot_path(path_id pid) const
  {
    auto idx = find_entry_idx(pid);
    if (idx >= 0)
    {
      return get_depot_path(m_entries[idx]);
    }
    return std::nullopt;
  }

  std::optional<path> get_path(path_id pid) const
  {
    auto idx = find_entry_idx(pid);
    if (idx >= 0)
    {
      return get_path(m_entries[idx]);
    }
    return std::nullopt;
  }

  bool has_entry(path_id pid) const
  {
    return find_entry_idx(pid) >= 0;
  }

  const std::vector<std::shared_ptr<archive>>& archives() const
  {
    return m_archives;
  }

  void debug_check();

protected:

  int32_t find_entry_idx(path_id pid) const
  {
    auto it = m_pidlinks.find(pidlink{pid, 0});
    if (it != m_pidlinks.end())
    {
      return it->entry_idx;
    }
    return -1;
  }

  // using an entry from another tree would be a mistake
  // so (const entry& e) variants are protected

  std::optional<path> get_depot_path(const entry& e) const
  {
    if (e.flags & entry::flag::has_depot_path)
    {
      return get_path(e);
    }

    return std::nullopt;
  }

  path get_parent_path(const entry& e) const
  {
    if (e.parent_entry_idx >= 0)
    {
      return get_path(m_entries[e.parent_entry_idx]);
    }
    return {};
  }

  path get_path(const entry& e) const
  {
    path p;
    append_path(p, e);
    return p;
  }

  void append_path(path& p, const entry& e) const
  {
    if (e.kind != entry_kind::root && e.kind != entry_kind::none)
    {
      assert(e.parent_entry_idx >= 0); // only root has no parent
      append_path(p, m_entries[e.parent_entry_idx]);
      p /= path(e.name.strv(), path::already_normalized_tag{});
    }
  }

  // pair's second is set to true if insertion happened
  std::pair<int32_t, bool> insert_child_entry(int32_t parent_entry_idx, fs_gname name, entry_kind type, bool is_depot_path);

  void compute_info() const
  {
    if (m_cached_info_dirty)
    {
      m_total_compressed_size = 0;
      m_total_size = 0;
      for (const auto& e : m_entries)
      {
        if (e.kind == entry_kind::file)
        {
          auto finfo = m_archives[e.archive_idx]->get_file_info(e.file_idx);
          m_total_compressed_size += finfo.disk_size;
          m_total_size += finfo.size;
        }
      }

      m_cached_info_dirty = false;
    }
  }

  bool is_valid_entry_index(int32_t idx) const
  {
    return idx >= 0 && idx < m_entries.size();
  }

private:

  std::vector<entry> m_entries;

  struct pidlink
  {
    pidlink(path_id pid, int32_t entry_idx)
      : pid(pid), entry_idx(entry_idx) {}

    path_id pid;
    int32_t entry_idx;
  };

  struct pidlink_hash {
    size_t operator()(const pidlink& x) const { return x.pid.hash; }
  };

  struct pidlink_eq {
    size_t operator()(const pidlink& a, const pidlink& b) const { return a.pid.hash == b.pid.hash; }
  };

  std::unordered_set<pidlink, pidlink_hash, pidlink_eq> m_pidlinks;

  std::vector<std::shared_ptr<archive>> m_archives;
  bool m_full = false;

  mutable size_t m_total_compressed_size = 0;
  mutable size_t m_total_size = 0; // with files' first segment decompressed
  mutable bool m_cached_info_dirty = true;
};

} // namespace redx::filesystem

