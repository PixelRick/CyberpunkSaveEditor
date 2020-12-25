#pragma once

#include <iostream>
#include <string>
#include <map>
#include "utils.hpp"

struct namehash
{
  namehash() = default;

  explicit namehash(const std::string& name)
  {
    as_u64 = 0;
    CRC32 c {};
    c.feed(name.data(), name.size());
    crc = c.get();
    slen = (uint8_t)name.size();
  }

  union {
    struct {
      uint32_t crc;
      uint8_t slen;
      uint8_t uk[2];
    };
    uint64_t as_u64;
  };

  friend std::istream& operator>>(std::istream& is, namehash& id)
  {
    is.read((char*)&id.as_u64, 7);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const namehash& id)
  {
    os.write((char*)&id.as_u64, 7);
    return os;
  }
};

class cpnames
{
  std::vector<std::string> m_namelist;
  std::map<uint64_t, std::string> m_namemap;

private:
  cpnames()
  {
    m_namelist = {
      #include "names.inl"
    };

    CRC32 crc;
    for (auto s : m_namelist)
    {
      namehash id(s);
      m_namemap[id.as_u64] = s;
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

  bool is_cp_name(const std::string& s) const
  {
    namehash id(s);
    return m_namemap.find(id.as_u64) != m_namemap.end();
  }

  std::string get_name(const namehash& id) const
  {
    auto it = m_namemap.find(id.as_u64);
    if (it != m_namemap.end())
      return it->second;
    std::stringstream ss;
    ss << "unknown_name_" << id.as_u64;
    return ss.str();
  }
};

