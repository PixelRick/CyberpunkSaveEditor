#pragma once
#include <redx/filesystem/path.hpp>
#include <redx/filesystem/treefs.hpp>
#include <redx/filesystem/directory_entry.hpp>

namespace redx::filesystem {

// let's assume that the treefs is stable once loaded
// so that we don't have to do complicated synchronization
struct directory_iterator
{
  using path_type         = redx::filesystem::path;
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

  inline reference operator*() const
  {
    return m_dirent;
  }

  inline pointer operator->() const
  {
    return &m_dirent;
  }

  inline directory_iterator& operator++()
  {
    m_dirent.assign_next();
    return *this;
  }

  inline bool operator==(const directory_iterator& rhs) const
  {
    return (*this)->pid() == rhs->pid();
  }

  inline bool operator!=(const directory_iterator& rhs) const
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
  using path_type         = redx::filesystem::path;
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

} // redx::filesystem

