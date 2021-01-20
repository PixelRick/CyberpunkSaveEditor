#pragma once
#include <fstream>
#include "cpinternals/common.hpp"

namespace cp {

// Output binary file archive
struct ofarchive
  : iarchive
{
  ofarchive() = default;

  ofarchive(std::filesystem::path path)
    : m_ofs(path, std::ios_base::binary)
  {
  }

  ofarchive(const char* filename)
    : m_ofs(filename, std::ios_base::binary)
  {
  }

  ~ofarchive() override = default;

  void open(std::filesystem::path path)
  {
    m_ofs.open(path, std::ios_base::binary);
  }

  void open(const char* filename)
  {
    m_ofs.open(filename, std::ios_base::binary);
  }

  void close()
  {
    m_ofs.close();
  }

  bool is_reader() const override
  {
    return false;
  }

  virtual pos_type tell() const override
  {
    return m_ofs.tellp();
  }

  virtual iarchive& seek(pos_type pos) override
  {
    m_ofs.seekp(pos);
    return *this;
  }

  virtual iarchive& seek(off_type off, std::istream::seekdir dir) override
  {
    m_ofs.seekp(off, dir);
    return *this;
  }

  virtual iarchive& serialize(void* data, size_t len) override
  {
    m_ofs.write(static_cast<char*>(data), len);
    return *this;
  }

protected:
  mutable std::ofstream m_ofs;
};

} // namespace cp

