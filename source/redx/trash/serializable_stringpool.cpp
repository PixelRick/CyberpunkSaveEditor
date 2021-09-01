//#include "stringpool.hpp"
//#include "iarchive.hpp"
//
//namespace redx {
//
//bool stringpool::serialize_in(iarchive& reader, uint32_t descs_size, uint32_t pool_size, uint32_t descs_offset = 0)
//{
//  if (descs_size % sizeof(detail::range_desc) != 0)
//    return false;
//
//  const uint32_t data_offset = descs_offset + descs_size;
//
//  const size_t descs_cnt = descs_size / sizeof(CRangeDesc);
//  if (descs_cnt == 0)
//    return pool_size == 0;
//
//  m_descs.resize(descs_cnt);
//  reader.read((char*)m_descs.data(), descs_size);
//  uint32_t max_offset = 0;
//  // reoffset offsets
//  for (auto& desc : m_descs)
//  {
//    desc.offset(desc.offset() - data_offset);
//    const uint32_t end_offset = desc.end_offset();
//    if (end_offset > max_offset)
//      max_offset = end_offset;
//  }
//
//  // iterator is valid
//  const uint32_t real_end_offset = descs_size + pool_size;
//  if (max_offset > real_end_offset)
//    return false;
//
//  m_buffer.resize(data_size);
//  reader.read(m_buffer.data(), pool_size);
//
//  // fill acceleration structure
//  m_sorted_desc_indices.resize(m_descs.size());
//  std::iota(m_sorted_desc_indices.begin(), m_sorted_desc_indices.end(), 0);
//
//  std::stable_sort(m_sorted_desc_indices.begin(), m_sorted_desc_indices.end(),
//    [this](size_t i1, size_t i2){ return view_from_idx((uint32_t)i1) < view_from_idx((uint32_t)i2); });
//
//  return true;
//}
//
//bool stringpool::serialize_out(iarchive& writer, uint32_t& descs_size, uint32_t& pool_size)
//{
//  descs_size = (uint32_t)(m_descs.size() * sizeof(CRangeDesc));
//
//  std::vector<CRangeDesc> new_descs = m_descs;
//  // reoffset offsets
//  for (auto& desc : new_descs)
//    desc.offset(desc.offset() + descs_size);
//
//  writer.write((char*)new_descs.data(), descs_size);
//  pool_size = (uint32_t)m_buffer.size();
//  writer.write((char*)m_buffer.data(), pool_size);
//  return true;
//}
//
//} // namespace redx
//
