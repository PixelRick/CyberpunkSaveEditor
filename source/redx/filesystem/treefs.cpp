#include <redx/filesystem/treefs.hpp>
#include <redx/io/file_stream.hpp>
#include <filesystem>

namespace redx::filesystem {

bool treefs::load_archive(const std::filesystem::path& path)
{
  if (m_full)
  {
    SPDLOG_ERROR("max count of archives already reached");
    return false;
  }

  if (!std::filesystem::is_regular_file(path))
  {
    SPDLOG_ERROR("invalid path");
    return false;
  }

  for (const auto& ar : m_archives)
  {
    if (ar->path() == path)
    {
      SPDLOG_ERROR("an archive with same path has already been loaded");
      return false;
    }
  }

  auto ar = redx::archive::load(path);
  if (!ar)
  {
    SPDLOG_ERROR("couldn't load archive");
    return false;
  }

  uint16_t ar_idx = static_cast<uint16_t>(m_archives.size());
  if (ar_idx == std::numeric_limits<uint16_t>::max())
  {
    m_full = true;
  }
  m_archives.emplace_back(ar);

  auto ardb_path = std::filesystem::path("./ardbs/") / path.filename().replace_extension("ardb");
  if (!std::filesystem::is_regular_file(ardb_path))
  {
    SPDLOG_WARN("{} is missing, all files from loaded archive will only be accessible by file_id", ardb_path.string());
  }
  else
  {
    load_ardb(ardb_path);
  }

  // map tree records to archive records

  int32_t file_idx = -1;
  for (const auto& ar_rec : ar->records())
  {
    ++file_idx;

    const path_id pid(ar_rec.fid);
    int32_t entry_idx = find_entry_idx(pid);
    bool is_override = true;

    if (entry_idx < 0)
    {
      entry_idx = insert_child_entry(unids_idx, fs_gname(fmt::format("{:016x}.bin", pid.hash())), entry_kind::file, false).first;
      if (entry_idx < 0)
      {
        // collision, already logged by insert
        continue;
      }

      // also add the real pid
      m_pidlinks.emplace(pid, entry_idx);
      is_override = false;
    }

    auto& e = m_entries[entry_idx];

    if (e.is_reserved_for_file())
    {
      is_override = false;
      e.kind = entry_kind::file;
    }
    else if (!e.is_file())
    {
      SPDLOG_ERROR("file's pid matches a non-file entry");
      continue;
    }

    if (is_override)
    {
      e.override_cnt += 1;
    }

    e.file_idx = file_idx;
    e.archive_idx = ar_idx;
  }

  m_cached_info_dirty = true;
  return true;
}


struct ardb_header
{
  uint32_t magic = 'ARDB';
  uint32_t names_cnt;
  uint32_t dirnames_cnt;
  uint32_t entries_cnt;

  bool is_magic_ok() const
  {
    return magic == 'ARDB';
  }
};

struct ardb_record
{
  uint32_t name_idx;
  int32_t parent_idx;
};

constexpr int32_t ardb_root_idx = -1;

// ardb format (custom thing, not cdpr's)
// string  fnames (path components)
// array of (dirs/files fhash (optional), idx parent, idx fname) = one u64 per file/ folder = 5MB
bool treefs::load_ardb(const std::filesystem::path& arpath)
{
  redx::file_istream ifs;
  ifs.open(arpath);

  if (!ifs.is_open())
  {
    SPDLOG_ERROR("ardb file not found at {}", arpath.string());
    return false;
  }

  ardb_header hdr;
  ifs.serialize_pod_raw(hdr);

  if (!hdr.is_magic_ok())
  {
    SPDLOG_ERROR("ardb file has wrong magic");
    return false;
  }

  std::vector<fs_gname> names; // directory names are first
  names.reserve(hdr.names_cnt);

  std::string name;
  for (size_t i = 0; i < hdr.names_cnt; ++i)
  {
    ifs.serialize_str_lpfxd(name);
    names.emplace_back(name);
  }

  std::vector<ardb_record> recs(hdr.entries_cnt);
  ifs.serialize_pods_array_raw(recs.data(), hdr.entries_cnt);

  std::vector<int32_t> entry_indices;

  for (size_t i = 0; i < hdr.entries_cnt; ++i)
  {
    const ardb_record& rec = recs[i];
    const bool is_file = (rec.name_idx >= hdr.dirnames_cnt);

    int32_t parent_entry_idx;
    if (rec.parent_idx == ardb_root_idx)
    {
      parent_entry_idx = root_idx;
    }
    else if (rec.parent_idx >= i)
    {
      SPDLOG_ERROR("unordered record found");
      return false;
    }
    else
    {
      parent_entry_idx = entry_indices[rec.parent_idx];
    }

    fs_gname name = names[rec.name_idx];

    // temporary, ardbs have the root entry for some reason..
    if (name.strv() == "")
    {
      assert(parent_entry_idx == root_idx);
      entry_indices.emplace_back(root_idx);
      continue;
    }

    int32_t entry_index = insert_child_entry(parent_entry_idx, name, is_file ? entry_kind::reserved_for_file : entry_kind::directory, true).first;
    if (entry_index < 0)
    {
      SPDLOG_ERROR("failed to insert entry for {}", name.strv());
      return false;
    }

    entry_indices.emplace_back(entry_index);
  }

  return true;
}

void treefs::debug_check()
{
  for (int32_t idx = 0; idx < m_entries.size(); ++idx)
  {
    const auto& e = m_entries[idx];
    if (e.pid.is_null())
    {
      SPDLOG_ERROR("treefs's entry {} has null pid", get_path(e).strv());
    }

    SPDLOG_DEBUG("checking [{}] {}, parent:{}, next:{}, child:{}",
      idx, get_path(e).strv(), e.parent_entry_idx, e.next_entry_idx, e.first_child_entry_idx);

    int32_t parent_idx = e.parent_entry_idx;
    if (!is_valid_entry_index(parent_idx) && e.kind != entry_kind::root)
    {
      SPDLOG_ERROR("treefs's entry {} isn't root but has no parent", e.name.strv());
      continue;
    }

    if (e.kind != entry_kind::root)
    {
      const auto& parent = m_entries[parent_idx];
      int32_t search_idx = parent.first_child_entry_idx;
      bool found = false;
      while (!found && search_idx > 0)
      {
        if (search_idx == idx)
        {
          found = true;
          break;
        }
        search_idx = m_entries[search_idx].next_entry_idx;
      }

      if (!found)
      {
        SPDLOG_ERROR("treefs's entry {} isn't in its parent's children chain", get_path(e).strv());
        continue;
      }
    }
  }
}

std::pair<int32_t, bool> treefs::insert_child_entry(int32_t parent_entry_idx, fs_gname name, entry_kind type, bool is_depot_path)
{
  if (!is_valid_entry_index(parent_entry_idx))
  {
    SPDLOG_ERROR("parent_idx is invalid");
    return {-1, false};
  }

  // must do a copy here !
  entry parent_copy = m_entries[parent_entry_idx];
  if (parent_copy.kind != entry_kind::directory && parent_copy.kind != entry_kind::root)
  {
    SPDLOG_ERROR("parent_idx must correspond to an entry of type root or directory");
    return {-1, false};
  }

  const auto name_strv = name.strv();
  path_id pid = parent_copy.pid / path(name_strv, path::already_normalized_tag{});

  int32_t entry_idx = find_entry_idx(pid);
  if (entry_idx >= 0)
  {
    // todo: check parent is the same
    
    const auto& e = m_entries[entry_idx];
    if (e.name != name || e.parent_entry_idx != parent_entry_idx)
    {
      SPDLOG_CRITICAL(
        "path hash collision detected, \"{}\" vs \"{}\"",
        get_path(e).string(),
        (get_path(parent_copy).append(name_strv, path::already_normalized_tag())).string());
      return {-1, false};
    }

    //SPDLOG_DEBUG("treefs::insert_entry: entry already present {}", name.strv());
    return {entry_idx, false};
  }

  if (m_entries.size() > std::numeric_limits<int32_t>::max())
  {
    SPDLOG_ERROR("maximum number of entries reached");
    return {-1, false};
  }

  entry_idx = static_cast<int32_t>(m_entries.size());
  auto& new_entry = m_entries.emplace_back(pid, name, type, is_depot_path);

  new_entry.parent_entry_idx = parent_entry_idx;

  // insert in alpha order
  //int32_t e1 = parent_entry_idx;
  //int32_t e2 = parent_copy.first_child_entry_idx;
  //
  //while (e2 >= 0 && name_strv > m_entries[e2].name.strv())
  //{
  //  e1 = e2;
  //  e2 = m_entries[e2].next_entry_idx;
  //}
  //
  //if (e1 == parent_entry_idx)
  //{
    new_entry.next_entry_idx = parent_copy.first_child_entry_idx;
    m_entries[parent_entry_idx].first_child_entry_idx = entry_idx;
  //}
  //else
  //{
  //  auto& prev_entry = m_entries[e1];
  //  new_entry.next_entry_idx = prev_entry.next_entry_idx;
  //  prev_entry.next_entry_idx = entry_idx;
  //}

  m_pidlinks.emplace(pid, entry_idx);

  return {entry_idx, true};
}

} // namespace redx::filesystem

