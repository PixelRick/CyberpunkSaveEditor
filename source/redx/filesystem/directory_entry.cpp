#include <cpinternals/filesystem/directory_entry.hpp>

namespace cp::filesystem {

void directory_entry::assign_entry(int32_t entry_idx, bool refresh_tfs_path)
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
      m_info.time = 150507936000000000; // e-egg
      //m_info.time = 120290000000000000; // safe date
      m_info.size = 0;
      m_info.disk_size = 0;
    }

    if (refresh_tfs_path)
    {
      m_tfs_parent_path = m_tfs->get_parent_path(m_entry);
    }
  }
}

} // cp::filesystem

