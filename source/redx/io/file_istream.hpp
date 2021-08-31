#pragma once
#include <fstream>
#include <filesystem>
#include <redx/common.hpp>

namespace redx {

// Input binary file stream
struct file_istream
  : streambase
{
  file_istream() = default;
  ~file_istream() override = default;

  file_istream(std::filesystem::path path)
    : m_ifs(path, std::ios_base::binary)
  {
  }

  file_istream(const char* filename)
    : m_ifs(filename, std::ios_base::binary)
  {
  }

  file_istream(file_istream&& other)
    : m_ifs(std::move(other.m_ifs))
  {
  }

  file_istream& operator=(file_istream&& rhs)
  {
    m_ifs = std::move(rhs.m_ifs);
    return *this;
  }

  void swap(file_istream& other)
  {
    if (this != &other)
    {
        m_ifs.swap(other.m_ifs);
    }
  }

  void open(std::filesystem::path path)
  {
    m_ifs.open(path, std::ios_base::binary);
  }

  void open(const char* filename)
  {
    m_ifs.open(filename, std::ios_base::binary);
  }

  bool is_open() const
  {
    return m_ifs.is_open();
  }

  void close()
  {
    m_ifs.close();
  }

  bool is_reader() const override
  {
    return true;
  }

  pos_type tell() const override
  {
    return m_ifs.tellg();
  }

  streambase& seek(pos_type pos) override
  {
    m_ifs.seekg(pos);
    return *this;
  }

  streambase& seek(off_type off, seekdir dir) override
  {
    m_ifs.seekg(off, dir);
    return *this;
  }

  streambase& serialize_bytes(void* data, size_t size) override
  {
    m_ifs.read(static_cast<char*>(data), size);
    return *this;
  }

  void swap_underlying_stream(std::ifstream& other)
  {
    m_ifs.swap(other);
  }

protected:

  mutable std::ifstream m_ifs;
};

} // namespace redx

