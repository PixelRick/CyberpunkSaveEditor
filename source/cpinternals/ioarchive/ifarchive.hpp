#pragma once
#include <fstream>
#include <filesystem>
#include "cpinternals/common.hpp"

namespace cp {

// Input binary file archive
struct ifarchive
  : iarchive
{
  ifarchive() = default;

  ifarchive(std::filesystem::path path)
    : m_ifs(path, std::ios_base::binary)
  {
  }

  ifarchive(const char* filename)
    : m_ifs(filename, std::ios_base::binary)
  {
  }

  ~ifarchive() override = default;

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

  iarchive& seek(pos_type pos) override
  {
    m_ifs.seekg(pos);
    return *this;
  }

  iarchive& seek(off_type off, std::istream::seekdir dir) override
  {
    m_ifs.seekg(off, dir);
    return *this;
  }

  iarchive& serialize(void* data, size_t len) override
  {
    m_ifs.read(static_cast<char*>(data), len);
    return *this;
  }

protected:
  mutable std::ifstream m_ifs;
};

} // namespace cp

