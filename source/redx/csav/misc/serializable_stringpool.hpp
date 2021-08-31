//#pragma once
//#include <inttypes.h>
//#include <stdexcept>
//#include <vector>
//#include <array>
//#include <unordered_map>
//#include "iarchive.hpp"
//
//namespace redx {
//
//namespace detail {
//
//struct range_desc
//{
//  constexpr static size_t serial_size = sizeof(uint32_t);
//
//  range_desc() noexcept = default;
//
//  range_desc(uint32_t offset, uint32_t len)
//  {
//    this->offset(offset);
//    this->len(len);
//  }
//
//  uint32_t offset() const { return m_offsetlen & 0xFFFFFF; }
//  uint32_t len() const { return m_offsetlen >> 24; }
//
//  uint32_t end_offset() const { return offset() + len(); }
//
//  void offset(uint32_t value)
//  {
//    if (value > 0xFFFFFF)
//      throw std::range_error("range_desc: offset is too big");
//    m_offsetlen = value | (m_offsetlen & 0xFF000000);
//  }
//
//  void len(uint32_t value)
//  {
//    if (value > 0xFF)
//      throw std::range_error("range_desc: len is too big");
//    m_offsetlen = (value << 24) | (m_offsetlen & 0x00FFFFFF);
//  }
//
//  uint32_t as_u32() const { return m_offsetlen; }
//
//private:
//  uint32_t m_offsetlen = 0;
//};
//
//} // namespace detail
//
//
//class stringpool
//{
//  using block_type = std::array<char, 0x10000>;
//
//  stringpool()
//  {
//    m_buffer.reserve(0x1000);
//  }
//
//  static stringpool& get_global()
//  {
//    static stringpool instance;
//    return instance;
//  }
//
//  bool has_string(std::string_view s) const
//  {
//    return const_cast<stringpool*>(this)->to_idx(s, false) != (uint32_t)-1;
//  }
//
//  size_t size() const { return m_views.size(); }
//
//  uint32_t to_idx(std::string_view s, bool create_if_not_present=true)
//  {
//    size_t ssize = s.size();
//    const char* const pdata = m_buffer.data();
//
//    uint32_t idx = 0;
//    auto it = lower_bound_idx(s);
//    if (it != m_sorted_desc_indices.end())
//    {
//      idx = (uint32_t)*it;
//      auto lbound_str = view_from_idx(idx);
//      if (s == lbound_str)
//        return idx;
//    }
//
//    if (!create_if_not_present)
//      return (uint32_t)-1;
//
//    ssize = s.size() + 1;
//    if (ssize > 0xFF)
//      throw std::length_error("CStringPool: string is too big");
//
//    idx = (uint32_t)m_descs.size();
//    m_descs.emplace_back((uint32_t)m_buffer.size(), (uint32_t)ssize);
//    m_buffer.insert(m_buffer.end(), s.begin(), s.end());
//    m_buffer.push_back('\0');
//
//    m_sorted_desc_indices.insert(it, idx);
//    return idx;
//  }
//
//  std::string_view at(uint32_t idx) const
//  {
//    if (idx > m_views.size())
//      throw std::out_of_range("stringpool: index out of range");
//    return m_views[idx];
//  }
//
//  // These serialization methods are used by the csav's System nodes
//  bool serialize_in(iarchive& reader, uint32_t descs_size, uint32_t pool_size, uint32_t descs_offset = 0);
//  bool serialize_out(iarchive& writer, uint32_t& descs_size, uint32_t& pool_size);
//
//protected:
//  std::vector<std::unique_ptr<block_type>> m_blocks;
//  std::vector<std::string_view> m_views;
//
//  // accelerated search
//
//  std::unordered_map<std::string_view, size_t> m_viewsmap;
//};
//
//
//
//} // namespace redx
//
