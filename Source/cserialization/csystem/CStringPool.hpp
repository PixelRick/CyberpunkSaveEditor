#pragma once
#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <algorithm>

#include <utils.hpp>
#include <cserialization/serializers.hpp>


class CRangeDesc
{
  uint32_t m_offsetlen = 0;

public:
  constexpr static size_t serial_size = sizeof(uint32_t);

  CRangeDesc() = default;

  CRangeDesc(uint32_t offset, uint32_t len)
  {
    this->offset(offset);
    this->len(len);
  }

  uint32_t offset() const { return m_offsetlen & 0xFFFFFF; }
  uint32_t len() const { return m_offsetlen >> 24; }

  uint32_t end_offset() const { return offset() + len(); }

  void offset(uint32_t value)
  {
    if (value > 0xFFFFFF)
      throw std::range_error("CRangeDesc: offset is too big");
    m_offsetlen = value | (m_offsetlen & 0xFF000000);
  }

  void len(uint32_t value)
  {
    if (value > 0xFF)
      throw std::range_error("CRangeDesc: len is too big");
    m_offsetlen = (value << 24) | (m_offsetlen & 0x00FFFFFF);
  }

  uint32_t as_u32() const { return m_offsetlen; }
};

class CStringPool
{
public:
  // just got this idea.. never saw it but should work
  struct Friend {};
  friend struct Friend;

protected:
  std::vector<CRangeDesc> m_descs;
  std::vector<char> m_buffer;

public:
  CStringPool()
  {
    m_buffer.reserve(0x1000);
  }

  bool has_string(std::string_view s) const
  {
    return find_idx(s) != (uint32_t)-1;
  }

  uint32_t size() const { return (uint32_t)m_descs.size(); }

  uint32_t to_idx(std::string_view s)
  {
    uint32_t idx = find_idx(s);
    if (idx != (uint32_t)-1)
      return idx;

    const size_t ssize = s.size() + 1;
    if (ssize > 0xFF)
      throw std::length_error("CStringPool: string is too big");

    idx = (uint32_t)m_descs.size();
    m_descs.emplace_back((uint32_t)m_buffer.size(), (uint32_t)ssize);
    m_buffer.insert(m_buffer.end(), s.begin(), s.end());
    m_buffer.push_back('\0');

    return idx;
  }

  // string_view would not be safe since buffer can be reallocated
  std::string from_idx(uint32_t idx)
  {
    if (idx > m_descs.size())
      throw std::out_of_range("CStringPool: out of range idx");
    const auto& desc = m_descs[idx];
    return std::string(m_buffer.data() + desc.offset()/*, desc.len()*/); // should include null character
  }

protected:
  // returns (uint32_t)-1 if not found
  uint32_t find_idx(std::string_view s) const
  {
    const size_t ssize = s.size();
    const char* const pdata = m_buffer.data();
    uint32_t idx = 0;
    for (auto it = m_descs.begin(); it != m_descs.end(); ++it, ++idx)
    {
      if (it->len() == ssize && !std::memcmp(s.data(), pdata + it->offset(), ssize))
        return idx;
    }
    return (uint32_t)-1;
  }

public:
  bool serialize_in(std::istream& reader, uint32_t descs_size, uint32_t data_size)
  {
    if (descs_size % sizeof(CRangeDesc) != 0)
      return false;

    const size_t descs_cnt = descs_size / sizeof(CRangeDesc);
    if (descs_cnt == 0)
      return data_size == 0;

    m_descs.resize(descs_cnt);
    reader.read((char*)m_descs.data(), descs_size);
    uint32_t max_offset = 0;
    // reoffset offsets
    for (auto& desc : m_descs)
    {
      desc.offset(desc.offset() - descs_size);
      const uint32_t end_offset = desc.end_offset();
      if (end_offset > max_offset)
        max_offset = end_offset;
    }

    // iterator is valid
    const uint32_t real_end_offset = descs_size + data_size;
    if (max_offset > real_end_offset)
      return false;

    m_buffer.resize(data_size);
    reader.read(m_buffer.data(), data_size);

    return true;
  }

  bool serialize_out(std::ostream& writer, uint32_t& descs_size, uint32_t& pool_size)
  {
    descs_size = (uint32_t)(m_descs.size() * sizeof(CRangeDesc));

    std::vector<CRangeDesc> new_descs = m_descs;
    // reoffset offsets
    for (auto& desc : new_descs)
      desc.offset(desc.offset() + descs_size);

    writer.write((char*)new_descs.data(), descs_size);
    pool_size = (uint32_t)m_buffer.size();
    writer.write((char*)m_buffer.data(), pool_size);
    return true;
  }
};
