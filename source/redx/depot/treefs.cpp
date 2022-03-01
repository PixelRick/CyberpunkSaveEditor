#include <redx/depot/treefs.hpp>

#include <filesystem>
#include <string_view>

#include <redx/io/file_access.hpp>
#include <redx/io/file_bstream.hpp>
#include <redx/radr/srxl/srxl_format.hpp>

namespace redx::depot {

bool treefs::load_archive(const std::filesystem::path& p)
{
  if (m_full)
  {
    SPDLOG_ERROR("max count of archives already reached");
    return false;
  }

  if (!std::filesystem::is_regular_file(p))
  {
    SPDLOG_ERROR("invalid path");
    return false;
  }

  for (const auto& ar : m_archives)
  {
    if (ar->path() == p)
    {
      SPDLOG_ERROR("an archive with same path has already been loaded");
      return false;
    }
  }

  auto ar = archive::create();
  ar->open(p);

  if (!ar->is_open())
  {
    SPDLOG_ERROR("couldn't load archive");
    return false;
  }

  uint16_t ar_idx = reliable_integral_cast<uint16_t>(m_archives.size());
  if (ar_idx == std::numeric_limits<uint16_t>::max())
  {
    m_full = true;
  }
  m_archives.emplace_back(ar);

  auto srxl_path = std::filesystem::path("./pathdb/") / p.filename().replace_extension("srxl");
  if (!std::filesystem::is_regular_file(srxl_path))
  {
    SPDLOG_WARN("{} is missing, all files from loaded archive will only be accessible by file_id", srxl_path.string());
  }
  else
  {
    load_srxl(srxl_path);
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


// ardb format (custom thing, not cdpr's)
// string  fnames (path components)
// array of (dirs/files fhash (optional), idx parent, idx fname) = one u64 per file/ folder = 5MB
bool treefs::load_srxl(const std::filesystem::path& p)
{
  std_file_ibstream st(p);

  if (!st.is_open())
  {
    SPDLOG_ERROR("srxl file not found at {}", p.string());
    return false;
  }

  redx::radr::srxl::format::srxl srxl;
  srxl.serialize_in(st);

  if (!st)
  {
    return false;
  }

  struct path_dir
  {
    fnv1a64_t hash;
    std::string_view name;
    bool is_dir;
  };

  std::vector<path_dir> components_hashes;
  bool convert_success{};

  for (size_t i = 0; i < srxl.svec.size(); ++i)
  {
    auto strv = srxl.svec.at(i);
    path p(strv, convert_success);
    if (convert_success)
    {
      // don't register root..
      if (p.strv().empty())
      {
        continue;
      }

      strv = p.strv();
      fnv1a64_t hash = fnv1a64("");
      const char* const start = strv.data();
      const char* const end = start + strv.size() + 1; //+1 to get null character
      const char* prev = start;
      for (const char* pc = start; pc != end; ++pc)
      {
        if (*pc == '\\' || *pc == '\0')
        {
          std::string_view component_strv(prev, pc - prev);
          hash = fnv1a64_continue(hash, component_strv);
          components_hashes.emplace_back(path_dir{hash, component_strv, true});

          if (*pc == '\0')
          {
            break;
          }

          hash = fnv1a64_continue(hash, "\\");
          prev = pc + 1;
        }
      }
      components_hashes.back().is_dir = false;

      int32_t parent_entry_idx = root_idx;
      for (const auto& ch : components_hashes)
      {
        parent_entry_idx = insert_child_entry(parent_entry_idx, fs_gname(ch.name), ch.is_dir ? entry_kind::directory : entry_kind::reserved_for_file, true).first;
        if (parent_entry_idx < 0)
        {
          SPDLOG_ERROR("failed to insert entry for {}", ch.name);
          break;
        }
      }

      components_hashes.clear();
    }
    else
    {
      SPDLOG_ERROR("failed to fix srxl path: {}", strv);
    }
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

  entry_idx = reliable_integral_cast<int32_t>(m_entries.size());
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

} // namespace redx::depot

