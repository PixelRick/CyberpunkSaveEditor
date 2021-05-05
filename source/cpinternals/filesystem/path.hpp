#pragma once
#include <filesystem>
#include <string>

#include <cpinternals/common.hpp>

namespace cp::filesystem {

// depot paths:
// - are lower case ascii
// - use the separator '\'
// - have no prefix separator
// does not support input strings with a root path (e.g. "C:\")
struct path
{
  struct already_normalized_tag {};

  path() noexcept = default;
  ~path() = default;

  path(const path& p) = default;
  path(path&& p) noexcept = default;

  path(std::string&& s) noexcept
    : str(std::move(s))
  {
    normalize();
  }

  path(std::wstring_view wcs, bool& success)
  {
    assign(wcs, success);
  }

  template <class Source>
  path(const Source& s)
    : str(s)
  {
    normalize();
  }

  template <class InputIt>
  path(InputIt first, InputIt last)
    : str(first, last)
  {
    normalize();
  }

  path(std::string_view strv, already_normalized_tag&&)
    : str(strv) {}

  path(std::string&& s, already_normalized_tag&&)
    : str(std::move(s)) {}

  path& operator=(const path& p) = default;
  path& operator=(path&& p) noexcept = default;
  
  path& operator=(std::string&& s)
  {
    str = std::move(s);
    normalize();
  }
  
  template <class Source>
  path& operator=(const Source& s)
  {
    str = s;
    normalize();
  }

  friend inline bool operator==(const path& lhs, const path& rhs) noexcept
  {
    return lhs.str == rhs.str;
  }

  friend inline bool operator!=(const path& lhs, const path& rhs) noexcept
  {
    return !(lhs == rhs);
  }

  friend inline bool operator<(const path& lhs, const path& rhs) noexcept
  {
    return lhs.str < rhs.str;
  }

  friend inline bool operator>(const path& lhs, const path& rhs) noexcept
  {
    return rhs.str < lhs.str;
  }

  friend inline bool operator<=(const path& lhs, const path& rhs) noexcept
  {
    return !(rhs.str < lhs.str);
  }

  friend inline bool operator>=(const path& lhs, const path& rhs) noexcept
  {
    return !(lhs.str < rhs.str);
  }

  friend inline path operator/(const path& lhs, const path& rhs)
  {
    path ret = lhs;
    ret /= rhs;
    return ret;
  }

  path& operator/=(const path& rhs)
  {
    append_unnormalized(rhs.str);
    return *this;
  }

  template <class Source>
  path& operator/=(const Source& s)
  {
    const size_t start = append_unnormalized(s);
    normalize(start);
    return *this;
  }

  template <class Source>
  path& assign(const Source& s) noexcept
  {
    str = s;
    normalize();
  }

  path& assign(std::wstring_view wcs, bool& success) noexcept
  {
    str.resize(wcs.length(), 0);

    auto wr_it = str.begin();
    for (auto rd_it = wcs.begin(); rd_it != wcs.end(); ++rd_it, ++wr_it)
    {
      const wchar_t wc = *rd_it;

      if (wc & 0xFF80)
      {
        success = false;
        clear();
        return *this;
      }

      *wr_it = static_cast<char>(wc);
    }

    success = true;
    normalize();
    return *this;
  }

  template <class Source>
  path& append(const Source& s)
  {
    return operator/=(s);
  }

  template <class InputIt>
  path& append(InputIt first, InputIt last)
  {
    const size_t start = append_unnormalized<InputIt>(first, last);
    normalize(start);
    return *this;
  }

  void clear() noexcept
  {
    str.clear();
  }

  path& remove_filename()
  {
    size_t pos = str.rfind('\\');
    pos = (pos == std::string::npos) ? 0 : pos;
    str.resize(pos);
  }

  path& replace_filename(const path& replacement)
  {
    const size_t pos = find_filename_pos();
    str.replace(pos, std::string::npos, replacement.str);
  }

  path& replace_extension(const path& replacement = path())
  {
    size_t pos = find_extension_pos();
    if (pos != std::string::npos)
    {
      str.resize(pos);
    }
    if (!replacement.str.empty())
    {
      if (replacement.str[0] != '.')
      {
        str.push_back('.');
      }
      str.append(replacement.str);
    }
    return *this;
  }

  void swap(path& other) noexcept
  {
    str.swap(other.str);
  }

  const char* c_str() const noexcept
  {
    return str.c_str();
  }

  operator std::string() const
  {
    return str;
  }

  const std::string& string() const
  {
    return str;
  }

  // name explicitly saying there is no copy
  const std::string& strv() const
  {
    return str;
  }

  int compare(const path& p) const noexcept
	{
    return str.compare(p.str);
  }

  // todo: normalize while comparing..

  int compare(const std::string& str) const noexcept
	{
    return str.compare(path(str));
  }

  int compare(std::string_view strv) const noexcept
	{
    return str.compare(path(strv));
  }

  int compare(const char* s) const noexcept
	{
    return str.compare(path(s));
  }

  path parent_path() const
  {
    size_t pos = find_filename_pos();
    // remove separator if there is one
    if (pos > 0)
    {
      --pos;
    }
    return path(std::string_view(str).substr(0, pos), already_normalized_tag{});
  }

  path filename() const
  {
    const size_t pos = find_filename_pos();
    return path(std::string_view(str).substr(pos), already_normalized_tag{});
  }

  path extension() const
  {
    const size_t pos = find_extension_pos();
    return path(std::string_view(str).substr(pos), already_normalized_tag{});
  }

  path stem() const
  {
    const size_t ext_pos = find_extension_pos();
    const size_t filename_pos = find_filename_pos(ext_pos);
    if (ext_pos != filename_pos)
    {
      return path(std::string_view(str).substr(filename_pos, ext_pos - filename_pos), already_normalized_tag{});
    }
    return path(std::string_view(str).substr(filename_pos), already_normalized_tag{});
  }

  bool empty() const noexcept
  {
    return str.empty();
  }

protected:

  size_t find_filename_pos(size_t offset = std::string::npos) const noexcept
  {
    size_t pos = str.rfind('\\', offset);
    return (pos == std::string::npos) ? 0 : pos + 1;
  }

  size_t find_extension_pos(size_t offset = std::string::npos) const noexcept
  {
    size_t pos = str.size();
    const char* const rend = str.data();
    const char* const rbeg = rend + (offset == std::string::npos ? pos : offset);
    for (const char* pc = rbeg; pc-- != rend;)
    {
      const char c = *pc;

      if (c == '\\')
      {
        // no dot in filename
        break;
      }

      if (c == '.')
      {
        if (&pc[-1] == rend || pc[-1] == '\\')
        {
          // stems
          break;
        }

        if (pc[-1] == '.' && (pc == rbeg - 1))
        {
          // *..
          if (&pc[-2] == rend || pc[-2] == '\\')
          {
            // "\.." and ".."
            break;
          }
        }

        pos = std::distance(rend, pc);
        break;
      }
    }
    return pos;
  }

  template <class InputIt>
  size_t append_unnormalized(InputIt first, InputIt last)
  {
    const size_t start = str.size();

    if (!start)
    {
      str.assign(first, last);
    }
    else
    {
      str.push_back('\\');
      str.append(first, last);
      // probably be similar in perf:
      //str.resize(start + 1 + std::distance(first, last));
      //str[start] = '\\';
      //std::copy(first, last, str.data() + start + 1);
    }

    return start;
  }

  template <class Source>
  size_t append_unnormalized(const Source& s)
  {
    return append_unnormalized(std::begin(s), std::end(s));
  }

  // note: removes prefix separators even when start != 0
  void normalize(size_t start = 0)
  {
    bool prev_was_sep = true; // skip prefix separator if any
    auto write_it = str.begin() + start;
    for (auto read_it = write_it; read_it != str.end(); ++read_it)
    {
      const char c = *read_it;
      if (c == '/' || c == '\\')
      {
        if (!prev_was_sep)
        {
          *(write_it++) = '\\';
          prev_was_sep = true;
        }
      }
      else if (c == ':')
      {
        DEBUG_BREAK();
      }
      else
      {
        *(write_it++) = std::tolower(c);
        prev_was_sep = false;
      }
    }

    auto rit = std::reverse_iterator(write_it);
    while (rit != str.rend() && *rit == '\\')
    {
      ++rit;
    }

    str.resize(std::distance(str.begin(), rit.base()));
  }

private:

  std::string str;
};

inline void swap(path& lhs, path& rhs) noexcept
{
  lhs.swap(rhs);
}


// path_id (fnv1a64 hash of depot-relative path)
struct path_id
{
  path_id() = default;

  constexpr explicit path_id(uint64_t hash) noexcept
    : hash(hash)
  {
    if (hash < 0x1000)
    {
      SPDLOG_INFO("LOL");
    }
  }

  constexpr explicit path_id(const path& p) noexcept
  {
    hash = fnv1a64(p.strv());
  }

  friend streambase& operator<<(streambase& st, path_id& x)
  {
    return st << x.hash;
  }

  inline bool is_null() const noexcept
  {
    return hash == 0;
  }

  static constexpr path_id root()
  {
    return path_id("");
  }

  // in-place compute the path identifier of the concatenation of this's path and rhs with a directory separator.
  // if this path_id is invalid, nothing happens.
  // if this path_id identifies the root path, this is equivalent to *this = path_id(rhs).
  path_id& operator/=(const path& p)
  {
    if (hash != 0)
    {
      if (hash != root().hash)
      {
        hash = fnv1a64_continue(hash, "\\");
      }
      hash = fnv1a64_continue(hash, p.strv());
    }
    return *this;
  }

  // compute the path identifier of this's path and rhs with a directory separator
  // if this path_id is invalid, an invalid path_id is returned
  // if this path_id identifies the root path, this is equivalent to path_id(rhs).
  inline path_id operator/(const path& p) const
  {
    path_id ret(*this);
    ret /= p;
    return ret;
  }

  inline constexpr bool operator==(const path_id& rhs) const
  {
    return hash == rhs.hash;
  }

  inline constexpr bool operator!=(const path_id& rhs) const
  {
    return !operator==(rhs);
  }

  uint64_t hash = 0;
};

} // namespace cp::filesystem

namespace std {

template <>
struct hash<cp::filesystem::path_id>
{
  std::size_t operator()(const cp::filesystem::path_id& k) const noexcept
  {
    return k.hash;
  }
};

} // namespace std

