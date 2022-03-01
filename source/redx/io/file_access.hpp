#pragma once
#include <filesystem>
#include <ios>
#include <memory>

#include <redx/core.hpp>
#include <redx/io/bstream.hpp>

namespace redx {

// unbuffered access to file
// the read/write are not meant to be used for serialization!
// wrap it in a buffered stream for that :)
struct file_access
{
  virtual ~file_access() = default;

  virtual bool is_open() const = 0;

  virtual bstreampos tell() const = 0;
  virtual std::streamsize size() const = 0;

  virtual bool seekpos(bstreampos pos) = 0;

  virtual bool seekoff(std::streamoff off, std::ios::seekdir dir)
  {
    std::streamoff new_off = 0;

    switch (dir)
    {
      case std::ios::cur:
        new_off = reliable_integral_cast<std::streamoff>(tell()) + off;
        break;
      case std::ios::end:
        new_off = size() + off;
        break;
      case std::ios::beg:
      default:
        new_off = off;
        break;
    }

    if (new_off >= 0)
    {
      return seekpos(reliable_integral_cast<bstreampos>(new_off));
    }

    return false;
  }

  inline bool read(char* dst, size_t count)
  {
    size_t rsize{};
    return read(dst, count, rsize) && rsize == count;
  }

  inline bool write(const char* src, size_t count)
  {
    size_t wsize{};
    return write(src, count, wsize) && wsize == count;
  }

  inline bool read(const std::span<char>& dst)
  {
    return read(dst.data(), dst.size());
  }

  inline bool write(const std::span<const char>& src)
  {
    return write(src.data(), src.size());
  }

  virtual bool read(char* dst, size_t count, size_t& out_count) = 0;

  virtual bool write(const char* src, size_t count, size_t& out_count)
  {
    return false;
  }
};

// unbuffered access to standard file system file (only implemented for windows atm)
struct std_file_access
  : file_access
{
  virtual bool open(const std::filesystem::path& p, std::ios::openmode mode)
  {
    if (open_impl(p, mode))
    {
      m_path = p;
      return true;
    }

    return false;
  }

  virtual bool close() = 0;

  inline const std::filesystem::path& path() const
  {
    return m_path;
  }

  static std::unique_ptr<std_file_access> create();
  static std::unique_ptr<std_file_access> create(const std::filesystem::path& p, std::ios::openmode mode);

protected:

  virtual bool open_impl(const std::filesystem::path& p, std::ios::openmode mode) = 0;

private:

  std::filesystem::path m_path;
};

} // namespace redx

