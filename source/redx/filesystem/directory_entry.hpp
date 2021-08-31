#pragma once
#include <redx/core/path.hpp>
#include <redx/filesystem/treefs.hpp>

namespace redx::filesystem {

// this is a sensible struct
// needs as much optimization as it can
struct directory_entry
{
  using path_type = redx::path;

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
    m_path.assign(fmt::format("{:016x}", pid.hash()), path::already_normalized_tag());

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
  inline redx::archive::file_id archive_file_id() const
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
  inline const redx::archive::file_info& get_file_info() const
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
      m_tfs_parent_path.append(m_entry.name.strv(), path::already_normalized_tag());
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

  void assign_entry(int32_t entry_idx, bool refresh_tfs_path = true);

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

} // redx::filesystem

