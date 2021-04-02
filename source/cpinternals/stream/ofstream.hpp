#pragma once
#include <fstream>
#include <filesystem>
#include <cpinternals/common.hpp>

namespace cp {

// Output binary file stream
struct ofstream
  : streambase
{
  ofstream() = default;

  ofstream(std::filesystem::path path)
    : m_ofs(path, std::ios_base::binary)
  {
  }

  ofstream(const char* filename)
    : m_ofs(filename, std::ios_base::binary)
  {
  }

  ~ofstream() override = default;

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

  virtual streambase& seek(pos_type pos) override
  {
    m_ofs.seekp(pos);
    return *this;
  }

  virtual streambase& seek(off_type off, seekdir dir) override
  {
    m_ofs.seekp(off, dir);
    return *this;
  }

  virtual streambase& serialize(void* data, size_t size) override
  {
    m_ofs.write(static_cast<char*>(data), size);
    return *this;
  }

protected:

  mutable std::ofstream m_ofs;
};

} // namespace cp

