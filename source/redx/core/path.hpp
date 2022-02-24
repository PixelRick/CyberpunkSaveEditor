#pragma once
#include <redx/core/platform.hpp>

#include <string>
#include <string_view>
#include <type_traits>

#include <redx/core.hpp>

namespace redx {

// CRITICAL: this must remain AFAP!
//   this versus std::system::path is night and day for CPFS
//   everything has to be called at least half a million times

// resource paths:
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

  template <class InputIt>
  path(InputIt first, InputIt last, bool& success)
  {
    normalize_append(first, last, success);
  }

  template <class Source>
  path(const Source& s, bool& success)
    : path(std::begin(s), std::end(s), success) {}

  path(std::string&& str, bool& success)
    : str(std::move(str))
  {
    normalize_moved(success);
  }

  template <class Source>
  path(const Source& s, already_normalized_tag&&)
    : str(s) {}

  path(std::string&& s, already_normalized_tag&&)
    : str(std::move(s)) {}

  path& operator=(const path& p) = default;
  path& operator=(path&& p) noexcept = default;

  friend FORCE_INLINE bool operator==(const path& lhs, const path& rhs) noexcept
  {
    return lhs.str == rhs.str;
  }

  friend FORCE_INLINE bool operator!=(const path& lhs, const path& rhs) noexcept
  {
    return !(lhs == rhs);
  }

  friend FORCE_INLINE bool operator<(const path& lhs, const path& rhs) noexcept
  {
    return lhs.str < rhs.str;
  }

  friend FORCE_INLINE bool operator>(const path& lhs, const path& rhs) noexcept
  {
    return rhs.str < lhs.str;
  }

  friend FORCE_INLINE bool operator<=(const path& lhs, const path& rhs) noexcept
  {
    return !(rhs.str < lhs.str);
  }

  friend FORCE_INLINE bool operator>=(const path& lhs, const path& rhs) noexcept
  {
    return !(lhs.str < rhs.str);
  }

  friend FORCE_INLINE path operator/(const path& lhs, const path& rhs)
  {
    path ret = lhs;
    ret /= rhs;
    return ret;
  }

  FORCE_INLINE path& operator/=(const path& rhs)
  {
    append_checked(rhs.str);
    return *this;
  }

  template <class InputIt>
  FORCE_INLINE path& assign(InputIt first, InputIt last, bool& success)
  {
    str.clear();
    append(first, last, success);
    return *this;
  }

  template <class Source>
  FORCE_INLINE path& assign(const Source& src, bool& success)
  {
    return assign(std::begin(src), std::end(src), success);
  }

  template <class InputIt>
  FORCE_INLINE path& assign(InputIt first, InputIt last, already_normalized_tag&&)
  {
    str.clear();
    append(first, last, already_normalized_tag{});
    return *this;
  }

  template <class Source>
  FORCE_INLINE path& assign(const Source& src, already_normalized_tag&&)
  {
    return assign(std::begin(src), std::end(src), already_normalized_tag{});
  }

  template <class InputIt>
  FORCE_INLINE path& append(InputIt first, InputIt last, bool& success)
  {
    normalize_append(first, last, success);
    return *this;
  }

  template <class Source>
  FORCE_INLINE path& append(const Source& src, bool& success)
  {
    return append(std::begin(src), std::end(src), success);
  }

  template <class InputIt>
  FORCE_INLINE path& append(InputIt first, InputIt last, already_normalized_tag&&)
  {
    append_checked(first, last);
    return *this;
  }

  template <class Source>
  FORCE_INLINE path& append(const Source& src, already_normalized_tag&&)
  {
    return append(std::begin(src), std::end(src), already_normalized_tag{});
  }

  FORCE_INLINE void clear() noexcept
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

  FORCE_INLINE void swap(path& other) noexcept
  {
    str.swap(other.str);
  }

  FORCE_INLINE const char* c_str() const noexcept
  {
    return str.c_str();
  }

  FORCE_INLINE operator std::string() const noexcept
  {
    return str;
  }

  FORCE_INLINE std::string string() const noexcept
  {
    return str;
  }

  // name explicitly saying there is no copy
  FORCE_INLINE std::string_view strv() const& noexcept
  {
    return str;
  }

  FORCE_INLINE int compare(const path& p) const noexcept
	{
    return str.compare(p.str);
  }

  template <class Source>
  int compare(const Source& src) const noexcept
	{
    bool normalized = {};
    path p(src, normalized);
    return normalized ? compare(p) : -1;
  }

  FORCE_INLINE int compare(const char* s) const noexcept
	{
    return compare(std::string_view(s));
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

  FORCE_INLINE bool empty() const noexcept
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

  // returns previous size
  template <class InputIt>
  size_t append_checked(InputIt first, InputIt last)
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
  FORCE_INLINE path& append_checked(const Source& src)
  {
    append_checked(std::begin(src), std::end(src));
    return *this;
  }

  template <typename CharT>
  static FORCE_INLINE bool is_separator(CharT c)
  {
    return c == static_cast<CharT>('/') || c == static_cast<CharT>('\\');
  }

  // returns appended count of characters
  template <class InputIt, class OutIt>
  static size_t normalize_copy_nosep(InputIt first, InputIt last, OutIt dest, bool& success)
  {
    using input_char_type = decltype(*InputIt());

    success = true;

    bool prev_is_sep = true;

    auto write_it = dest;
    auto read_it = first;
    auto last_sep_it = dest;

    while (read_it != last)
    {
      const auto tc = *read_it++;

      // check for forbidden characters
      if (tc & 0xFF80)
      {
        success = false;
        return 0;
      }

      const char c = static_cast<char>(tc);

      if (c == '/' || c == '\\')
      {
        if (!prev_is_sep)
        {
          prev_is_sep = true;
          last_sep_it = write_it;
          *(write_it++) = '\\';
        }
      }
      else if (c == ':')
      {
        //DEBUG_BREAK();
        success = false;
        return 0;
      }
      else
      {
        prev_is_sep = false;
        *(write_it++) = std::tolower(c);
      }
    }

    if (prev_is_sep)
    {
      write_it = last_sep_it;
    }

    return std::distance(dest, write_it);
  }

  template <class InputIt>
  void normalize_append(InputIt beg, InputIt end, bool& success)
  {
    success = true;

    const size_t max_append = 1 + std::distance(beg, end);
    const size_t prev_size = str.size();

    str.resize(prev_size + max_append);

    auto write_it = str.begin() + prev_size;

    size_t appended_cnt = 0;

    if (prev_size > 0)
    {
      *(write_it++) = '\\';
      appended_cnt++;
    }

    const size_t copied_cnt = normalize_copy_nosep(beg, end, write_it, success);

    if (!success || !copied_cnt)
    {
      str.resize(prev_size);
    }
    else
    {
      appended_cnt += copied_cnt;
      str.resize(prev_size + appended_cnt);
    }
  }

  void normalize_moved(bool& success)
  {
    size_t copied_cnt = normalize_copy_nosep(str.begin(), str.end(), str.begin(), success);
    if (!success)
    {
      copied_cnt = 0;
    }
    str.resize(copied_cnt);
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
  path_id() noexcept = default;

  path_id(const path_id& other) noexcept = default;
  path_id& operator=(const path_id& other) noexcept = default;

  constexpr explicit path_id(fnv1a64_t hash) noexcept
    : m_hash(hash)
  {
    if (hash < 0x1000)
    {
      SPDLOG_INFO("LOL");
    }
  }

  constexpr explicit path_id(const path& p) noexcept
  {
    m_hash = fnv1a64(p.strv());
  }

  // in-place compute the path identifier of the concatenation of this's path and rhs with a directory separator.
  // if this path_id is invalid, nothing happens.
  // if this path_id identifies the root path, this is equivalent to *this = path_id(rhs).
  path_id& operator/=(const path& p)
  {
    if (m_hash != 0)
    {
      if (m_hash != root().hash())
      {
        m_hash = fnv1a64_continue(m_hash, "\\");
      }
      m_hash = fnv1a64_continue(m_hash, p.strv());
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

  FORCE_INLINE bool operator==(const path_id& rhs) const
  {
    return m_hash == rhs.m_hash;
  }

  FORCE_INLINE bool operator!=(const path_id& rhs) const
  {
    return !operator==(rhs);
  }

  FORCE_INLINE fnv1a64_t hash() const
  {
    return m_hash;
  }

  FORCE_INLINE bool is_null() const noexcept
  {
    return m_hash == 0;
  }

  static constexpr path_id root()
  {
    return path_id(""_fnv1a64);
  }

  /*path_id combine(const path_id& rhs)
  {
  }*/

  using path_already_normalized_tag = path::already_normalized_tag;

private:

  fnv1a64_t m_hash = 0;
};

// check serialization reqs
static_assert(std::is_trivially_assignable_v<path_id&, const path_id&>);
static_assert(sizeof(path_id) == 8);

} // namespace redx

template <>
struct std::hash<redx::path_id>
{
  std::size_t operator()(const redx::path_id& x) const noexcept
  {
    return x.hash();
  }
};

