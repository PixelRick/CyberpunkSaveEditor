#pragma once
#include <fstream>
#include <filesystem>
#include <cpinternals/common.hpp>

namespace cp {

// Input binary file stream
struct ifstream
  : streambase
{
  ifstream() = default;

  ifstream(std::filesystem::path path)
    : m_ifs(path, std::ios_base::binary)
  {
  }

  ifstream(const char* filename)
    : m_ifs(filename, std::ios_base::binary)
  {
  }

  ~ifstream() override = default;

  void open(std::filesystem::path path)
  {
    m_ifs.open(path, std::ios_base::binary);
  }

  void open(const char* filename)
  {
    m_ifs.open(filename, std::ios_base::binary);
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

  streambase& serialize(void* data, size_t size) override
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

} // namespace cp

