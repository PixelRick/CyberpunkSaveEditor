#pragma once
#include <filesystem>
#include <memory>

#include <cpinternals/common.hpp>

namespace cp {

struct file_reader_impl
{
  virtual ~file_reader_impl() = default;

  virtual bool open(const std::filesystem::path& p) = 0;
  virtual bool is_open() const = 0;
  virtual bool seek(size_t offset) = 0;
  virtual bool read(std::span<char> dst) = 0;
  virtual bool close() = 0;
};

struct file_reader
{
  file_reader();
  ~file_reader();

  file_reader(file_reader&&) = default;

  inline bool open(const std::filesystem::path& p)
  {
    return m_impl->open(p);
  }

  inline bool is_open() const
  {
    return m_impl->is_open();
  }

  inline bool seek(size_t offset) 
  {
    return m_impl->seek(offset);
  }

  inline bool read(const std::span<char>& dst)
  {
    return m_impl->read(dst);
  }

  inline bool close()
  {
    return m_impl->close();
  }

private:

  std::unique_ptr<file_reader_impl> m_impl;
};

} // namespace cp

