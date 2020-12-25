#pragma once

#include <string>
#include <map>
#include "utils.hpp"

class cpnames
{
  std::vector<std::string> m_namelist;
  std::map<uint32_t, std::string> m_namemap;

private:
  cpnames()
  {
    m_namelist = {
      #include "names.inl"
    };

    CRC32 crc;
    for (auto s : m_namelist)
    {
      crc.reset();
      crc.feed(s.data(), s.size());
      m_namemap[crc.get()] = s;
    }
  }
  ~cpnames() {}

public:
  cpnames(const cpnames &) = delete;
  cpnames& operator=(const cpnames &) = delete;

  static cpnames& get()
  {
    static cpnames s;
    return s;
  }

  const std::map<uint32_t, std::string>&
  namemap() const { return m_namemap; }
};

