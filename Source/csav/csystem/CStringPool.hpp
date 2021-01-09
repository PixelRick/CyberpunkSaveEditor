#pragma once
#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <algorithm>
#include <numeric>

#include <utils.hpp>
#include <csav/serializers.hpp>


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


// todo, don't reallocate buffer: make subpools
// this will make serialization more complicated but at least I
// could use the pool for the editor..
class CStringPool
{
protected:
  std::vector<CRangeDesc> m_descs;
  std::vector<char> m_buffer;
  
  // accelerated search

  std::vector<size_t> m_sorted_desc_indices;

  struct search_value_t
  {
    const CStringPool* sp;
    std::string_view s;

    bool operator>(const size_t& idx) const {
      const auto& desc = sp->m_descs[idx];
      std::string_view other(sp->m_buffer.data() + desc.offset(), desc.len() - 1);
      const bool ret = s > other;
      return ret;
    }

    friend bool operator<(const size_t& idx, const search_value_t& sv) {
      return sv.operator>(idx);
    }
  };


public:
  CStringPool()
  {
    m_buffer.reserve(0x1000);
  }

  static CStringPool& get_global()
  {
    static CStringPool strpool;
    return strpool;
  }

  bool has_string(std::string_view s) const
  {
    return const_cast<CStringPool*>(this)->to_idx(s, false) != (uint32_t)-1;
  }

  uint32_t size() const { return (uint32_t)m_descs.size(); }

  uint32_t to_idx(std::string_view s, bool create_if_not_present=true)
  {
    size_t ssize = s.size();
    const char* const pdata = m_buffer.data();

    uint32_t idx = 0;
    auto it = lower_bound_idx(s);
    if (it != m_sorted_desc_indices.end())
    {
      idx = (uint32_t)*it;
      auto lbound_str = view_from_idx(idx);
      if (s == lbound_str)
        return idx;
    }

    if (!create_if_not_present)
      return (uint32_t)-1;

    ssize = s.size() + 1;
    if (ssize > 0xFF)
      throw std::length_error("CStringPool: string is too big");

    idx = (uint32_t)m_descs.size();
    m_descs.emplace_back((uint32_t)m_buffer.size(), (uint32_t)ssize);
    m_buffer.insert(m_buffer.end(), s.begin(), s.end());
    m_buffer.push_back('\0');

    m_sorted_desc_indices.insert(it, idx);
    return idx;
  }

  // string_view is safe atm since buffer can be reallocated
  std::string from_idx(uint32_t idx) const
  {
    if (idx > m_descs.size())
      throw std::out_of_range("CStringPool: out of range idx");
    const auto& desc = m_descs[idx];
    return std::string(m_buffer.data() + desc.offset()/*, desc.len()*/); // should include null character
  }

  std::string_view view_from_idx(uint32_t idx) const
  {
    if (idx > m_descs.size())
      throw std::out_of_range("CStringPool: out of range idx");
    const auto& desc = m_descs[idx];
    return std::string_view(m_buffer.data() + desc.offset()/*, desc.len()*/); // should include null character
  }

protected:
  std::vector<size_t>::const_iterator lower_bound_idx(std::string_view s) const
  {
    return std::lower_bound(
      m_sorted_desc_indices.begin(),
      m_sorted_desc_indices.end(),
      search_value_t{this, s}
    );
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

    // fill acceleration structure
    m_sorted_desc_indices.resize(m_descs.size());
    std::iota(m_sorted_desc_indices.begin(), m_sorted_desc_indices.end(), 0);

    std::stable_sort(m_sorted_desc_indices.begin(), m_sorted_desc_indices.end(),
      [this](size_t i1, size_t i2){ return view_from_idx((uint32_t)i1) < view_from_idx((uint32_t)i2); });

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

// let's use a global pool
class CSysName
{
  uint32_t m_idx;

public:
  CSysName()
  {
    m_idx = CStringPool::get_global().to_idx("uninitialized");
  }

  CSysName(const char* s)
    : CSysName(std::string_view(s)) {}

  CSysName(std::string_view s)
  {
    m_idx = CStringPool::get_global().to_idx(s);
  }

  CSysName(const CSysName&) = default;
  CSysName& operator=(const CSysName&) = default;

  // todo: switch to string_view when CStringPool doesnot reallocate mem
  std::string str() const
  {
    return CStringPool::get_global().from_idx(m_idx);
  }

  uint32_t idx() const { return m_idx; }

  friend inline bool operator<(const CSysName& a, const CSysName& b) {
    return a.m_idx < b.m_idx;
  }

  friend inline bool operator==(const CSysName& a, const CSysName& b) {
    return a.m_idx == b.m_idx;
  }

  friend inline bool operator!=(const CSysName& a, const CSysName& b) {
    return !(a == b);
  }
};

namespace std {

  template <>
  struct hash<CSysName>
  {
    std::size_t operator()(const CSysName& k) const
    {
      return hash<uint32_t>()(k.idx());
    }
  };

}

