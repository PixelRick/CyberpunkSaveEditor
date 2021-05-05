#pragma once
#include <filesystem>
#include <vector>
#include <shared_mutex>
#include <unordered_set>
#include <stack>

#include <cpinternals/common.hpp>
#include <cpinternals/filesystem/path.hpp>
#include <cpinternals/filesystem/archive.hpp>
#include <cpinternals/filesystem/archive_file_stream.hpp>

// to diff depots, the file system must be instantiatable
// goals:
//  - minimize memory usage for one instance (for the dokany-based project)
//  - provide std-like features to make it easy to learn&use
//  
//

namespace cp::filesystem {

constexpr auto unidentified_files_directory_name = "unidentified_files";

struct treefs;
struct directory_entry;
struct directory_iterator;
struct recursive_directory_iterator;

// fse: file system entry

using fs_gname = gstring<'RADR'>;
using file_info = cp::archive::file_info;
using file_handle = cp::archive::file_handle;
using file_stream = cp::archive_file_stream;

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


// this is a sensible struct
// needs as much optimization as it can
struct directory_entry
{
  using path_type = cp::filesystem::path;

  directory_entry() = default;

  explicit directory_entry(const treefs& tfs)
    : m_tfs(&tfs)
  {
  }

  directory_entry(const treefs& tfs, const path_type& p)
    : m_tfs(&tfs)
  {
    assign(p);
  }

  directory_entry(const treefs& tfs, path_id pid)
    : m_tfs(&tfs)
  {
    assign(pid);
  }

  void assign(const treefs& tfs, const path_type& p)
  {
    m_tfs = &tfs;
    assign(p);
  }

  void assign(const treefs& tfs, path_id pid)
  {
    m_tfs = &tfs;
    assign(pid);
  }

  void assign(const path_type& p)
  {
    m_by_pid = false;

    // special handling of pid strings
    uint64_t hash = 0;
    char dummy = 0;
    if (p.strv().size() == 16 && std::sscanf(p.c_str(), "%16llx%c", &hash, &dummy) == 1) // 2 would mean all characters weren't for the hex value
    {
      m_pid = path_id(hash);
      m_by_pid = true;
    }
    else
    {
      m_pid = path_id(p);
    }
    m_path = p;

    refresh();
  }

  void assign(path_id pid)
  {
    m_by_pid = true;
    m_pid = pid;
    m_path = fmt::format("{:016x}", pid.hash);

    refresh();
  }

  void refresh()
  {
    if (m_tfs != nullptr)
    {
      const auto entry_idx = m_tfs->find_entry_idx(m_pid);
      assign_entry(entry_idx);
    }
    else
    {
      // nothing to do as it should already be in default state
      // (there is no way for m_tfs to become nullptr other than by default construction)
    }
  }

  inline const treefs* tfs() const
  {
    return m_tfs;
  }

  inline path_id pid() const
  {
    return m_pid;
  }

  inline path_type path() const
  {
    return m_path;
  }

  inline path_type tfs_path() const
  {
    return m_tfs_parent_path / filename();
  }

  inline path_type tfs_parent_path() const
  {
    return m_tfs_parent_path;
  }

  inline path_type filename() const
  {
    return path_type(m_entry.name.strv(), path_type::already_normalized_tag{});
  }

  inline std::string_view filename_strv() const
  {
    return m_entry.name.strv();
  }

  std::optional<path_type> depot_path() const
  {
    if (exists() && m_entry.has_depot_path)
    {
      return tfs_path();
    }
    return std::nullopt;
  }

  // return value is undefined if this is not a file entry
  inline cp::archive::file_id archive_file_id() const
  {
    return m_info.id;
  }

  // this isn't an entry kind !
  // it is a property of the directory_entry
  inline bool is_pidlink() const
  {
    return m_by_pid && exists();
  }

  inline bool exists() const
  {
    return !m_entry.pid.is_null();
  }

  inline bool is_reserved_for_file() const
  {
    return m_entry.is_reserved_for_file();
  }

  inline bool is_directory() const
  {
    return m_entry.is_directory();
  }

  inline bool is_file() const
  {
    return m_entry.is_file();
  }

  // return value is undefined if this is not a file entry
  inline const cp::archive::file_info& get_file_info() const
  {
    return m_info;
  }

  // only root should have no parent..
  inline bool has_parent() const
  {
    return m_entry.parent_entry_idx >= 0;
  }

  void assign_parent()
  {
    assign_entry(m_entry.parent_entry_idx, false);
    if (exists())
    {
      m_tfs_parent_path = m_tfs_parent_path.parent_path();
      m_path = tfs_path();
      m_pid = path_id(m_path);
    }
    else
    {
      *this = directory_entry();
    }
  }

  inline bool has_next() const
  {
    return m_entry.next_entry_idx >= 0;
  }

  bool assign_next()
  {
    assign_entry(m_entry.next_entry_idx, false);
    if (exists())
    {
      // m_tfs_parent_path doesn't change :)
      m_path = tfs_path();
      m_pid = path_id(m_path);
      return true;
    }
    else
    {
      *this = directory_entry();
    }
    return false;
  }

  inline bool has_child() const
  {
    return m_entry.first_child_entry_idx >= 0;
  }

  void assign_first_child()
  {
    assign_entry(m_entry.first_child_entry_idx, false);
    if (exists())
    {
      if (!m_tfs_parent_path.empty())
      {
        m_tfs_parent_path.append("\\");
      }
      m_tfs_parent_path.append(m_entry.name.strv());
      m_path = tfs_path();
      m_pid = path_id(m_path);
    }
    else
    {
      *this = directory_entry();
    }
  }

  file_handle get_file_handle() const
  {
    return m_ar->get_file_handle(m_entry.file_idx);
  }

protected:

  void assign_entry(int32_t entry_idx, bool refresh_tfs_path = true)
  {
    m_ar = nullptr;

    if (!m_tfs || !m_tfs->is_valid_entry_index(entry_idx))
    {
      m_entry = treefs::entry();
      m_info.id = {};
      if (refresh_tfs_path)
      {
        m_tfs_parent_path.clear();
      }
    }
    else
    {
      const treefs::entry& e = m_tfs->m_entries[entry_idx];

      m_entry = e;

      if (e.is_file())
      {
        m_ar = m_tfs->archives()[e.archive_idx].get();
        m_info = m_ar->get_file_info(e.file_idx);
      }
      else
      {
        m_info.id = {};
        //m_info.time = 150290000000000000; // e-egg
        m_info.time = 120290000000000000; // e-egg
        m_info.size = 0;
        m_info.disk_size = 0;
      }

      if (refresh_tfs_path)
      {
        m_tfs_parent_path = m_tfs->get_parent_path(m_entry);
      }
    }
  }

private:

  const treefs* m_tfs = nullptr;
  path_id       m_pid;
  path_type     m_path;
  bool          m_by_pid = false;

  path_type     m_tfs_parent_path; // the path of the entry's directory in the tree
  treefs::entry m_entry;
  file_info     m_info;
  const archive* m_ar;
};

// let's assume that the treefs is stable once loaded
// so that we don't have to do complicated synchronization
struct directory_iterator
{
  using path_type         = cp::filesystem::path;
  using value_type        = directory_entry;
  using difference_type   = std::ptrdiff_t;
  using pointer           = const directory_entry*;
  using reference         = const directory_entry&;
  using iterator_category = std::input_iterator_tag;

  directory_iterator() = default;

  directory_iterator(const treefs& tfs, path_type path)
    : m_dirent(tfs)
  {
    directory_entry parent(tfs, path);
    if (parent.is_directory())
    {
      m_dirent = parent;
      m_dirent.assign_first_child();
      m_parent_path = m_dirent.tfs_path();
    }
  }

  //directory_iterator(const directory_iterator&) = default;
  //directory_iterator(directory_iterator&&) = default;
  //
  //directory_iterator& operator=(const directory_iterator&) = default;
  //directory_iterator& operator=(directory_iterator&&) = default;

  reference operator*() const
  {
    return m_dirent;
  }

  pointer operator->() const
  {
    return &m_dirent;
  }

  directory_iterator& operator++()
  {
    m_dirent.assign_next();
    return *this;
  }

  bool operator==(const directory_iterator& rhs) const
  {
    return (*this)->pid() == rhs->pid();
  }

  bool operator!=(const directory_iterator& rhs) const
  {
    return !operator==(rhs);
  }

private:

  friend struct recursive_directory_iterator;

  path_type m_parent_path;
  directory_entry m_dirent;
};

inline directory_iterator begin(directory_iterator iter) noexcept
{
  return iter;
}

inline directory_iterator end(const directory_iterator&) noexcept
{
  return directory_iterator();
}

// let's assume that the treefs is stable once loaded
// so that we don't have to do complicated synchronization
struct recursive_directory_iterator
{
  using path_type         = cp::filesystem::path;
  using value_type        = directory_entry;
  using difference_type   = std::ptrdiff_t;
  using pointer           = const directory_entry*;
  using reference         = const directory_entry&;
  using iterator_category = std::input_iterator_tag;

  recursive_directory_iterator() = default;

  recursive_directory_iterator(const treefs& tfs, path_type path)
    : m_dirit(tfs, path)
  {
  }

  //directory_iterator(const directory_iterator&) = default;
  //directory_iterator(directory_iterator&&) = default;
  //
  //directory_iterator& operator=(const directory_iterator&) = default;
  //directory_iterator& operator=(directory_iterator&&) = default;

  reference operator*() const
  {
    return *m_dirit;
  }

  pointer operator->() const
  {
    return &*m_dirit;
  }

  recursive_directory_iterator& operator++()
  {
    if (m_dirit->is_directory())
    {
      m_stacked.push(m_dirit);
      m_dirit.m_dirent.assign_first_child();
      m_dirit.m_parent_path = m_dirit->tfs_path();
    }
    else
    {
      ++m_dirit;

      while (m_dirit == end(m_dirit) && !m_stacked.empty())
      {
        assert(!m_dirit->has_next());
        m_dirit = m_stacked.top();
        m_stacked.pop();

        ++m_dirit;
      }
    }
    return *this;
  }

  bool operator==(const recursive_directory_iterator& rhs) const
  {
    return ((*this)->pid() == rhs->pid());
  }

  bool operator!=(const recursive_directory_iterator& rhs) const
  {
    return !operator==(rhs);
  }

private:

  directory_iterator m_dirit;
  std::stack<directory_iterator> m_stacked;
};

inline recursive_directory_iterator begin(recursive_directory_iterator iter) noexcept
{
  return iter;
}

inline recursive_directory_iterator end(const recursive_directory_iterator&) noexcept
{
  return recursive_directory_iterator();
}


} // namespace cp::filesystem

