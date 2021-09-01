#pragma once
#include <redx/core.hpp>
#include <redx/io/arfile_access.hpp>

namespace redx {

bool arfile_access::open(const std::shared_ptr<radr::archive>& archive, uint32_t index, option options)
{
  if (is_open())
  {
    SPDLOG_ERROR("already open");
    return false;
  }

  m_archive = archive;
  m_findex = index;
  m_options = options;
  m_finfo = m_archive->get_file_info(m_findex);
  m_buffer.clear();

  const auto& frec = m_archive->records()[m_findex];
  auto segs = frec.segs_irange.slice(m_archive->segments());

  m_segment_descs = std::vector<radr::segment_descriptor>(segs.begin(), segs.end());

  // here we could merge descriptors to tweak the buffering if desired
  // the game seems to buffer "inline buffers" with the first segment

  return true;
}

bool arfile_access::read_some(const std::span<char>& dst, size_t& rsize)
{
  rsize = 0;

  if (!is_open() || static_cast<std::streamoff>(m_pos) < 0)
  {
    return false;
  }

  const bstreampos pos = m_pos;
  const bstreampos end = pos + dst.size();

  if (end < pos || end > m_finfo.size)
  {
    // position overflow
    return false;
  }

  SPDLOG_DEBUG("pos:{:08X} end:{:08X}", pos, end);

  // try read from buffer
    
  if (pos >= m_buffer_pos && end <= m_buffer_pos + m_buffer.size())
  {
    const std::streamsize read_size = end - pos;

    auto beg = m_buffer.begin() + (pos - m_buffer_pos);
    std::copy(beg, beg + read_size, dst.begin());

    SPDLOG_DEBUG("read buffered");
    m_pos += read_size;
    rsize = read_size;
    return true;
  }

  // special behavior for first segment (decompression required)
  const auto& sd0 = m_segment_descs[0];
  if (pos < sd0.size)
  {
    if (pos == 0 && end >= sd0.size)
    {
      // skip buffering
      if (!m_archive->read_segment(sd0, dst.subspan(0, sd0.size), true))
      {
        SPDLOG_ERROR("couldn't read and decompress first segment");
        return false;
      }

      SPDLOG_DEBUG("read full sd0");
      m_pos += sd0.size;
      rsize = sd0.size;
      return true;
    }
    else
    {
      m_buffer_pos = 0;
      m_buffer.resize(sd0.size);
      if (!m_archive->read_segment(sd0, m_buffer, true))
      {
        m_buffer.clear();
        SPDLOG_ERROR("couldn't read and decompress first segment");
        return false;
      }

      const bstreampos read_end = std::min(static_cast<bstreampos>(sd0.size), end);
      const std::streamsize read_size = read_end - pos;

      auto beg = m_buffer.begin() + pos;
      std::copy(beg, beg + read_size, dst.begin());

      SPDLOG_DEBUG("read partial sd0");
      m_pos += read_size;
      rsize = read_size;
      return true;
    }
  }

  // search segment for pos

  auto segit = m_segment_descs.begin() + 1;
  bstreampos seg_offset = sd0.size;

  for (; segit != m_segment_descs.end(); ++segit)
  {
    const bstreampos end = seg_offset + segit->disk_size;

    if (pos < end)
    {
      break;
    }

    seg_offset = end;
  }

  if (segit == m_segment_descs.end())
  {
    SPDLOG_ERROR("logic error");
    return false;
  }

  bstreampos seg_end = seg_offset + segit->disk_size;

  SPDLOG_DEBUG(
    "seg_offset:{:08X} seg_end:{:08X}",
    seg_offset, seg_end);

  // do a buffered read if we can't skip/bulk
  if (end <= seg_end) // read is limited to the found segment
  {
    if (pos != seg_offset || end < seg_end) // read doesn't span the full segment
    {
      if (m_options & option::warn_on_partial_buffer_reads)
      {
        SPDLOG_ERROR("partial buffer read seg_offset:{:08X} seg_end:{:08X} read_offset:{:08X} read_end:{:08X}",
          seg_offset, seg_end, pos, end);
      }

      const auto& sd = *segit;

      // buffer segment
      m_buffer_pos = seg_offset;
      m_buffer.resize(sd.disk_size);
      if (!m_archive->read_segment(sd, m_buffer, false))
      {
        m_buffer.clear();
        SPDLOG_ERROR("couldn't read segment");
        return false;
      }

      const bstreampos read_end = end;
      const std::streamsize read_size = read_end - pos;

      assert(read_end <= seg_end);

      auto beg = m_buffer.begin() + (pos - seg_offset);
      std::copy(beg, beg + read_size, dst.begin());

      SPDLOG_DEBUG("read {:08X} bytes in buffered seg", read_size);
      m_pos += read_size;
      rsize = read_size;
      return true;
    }
  }

  // otherwise try bulk read

  radr::segment_descriptor bulk_sd = *segit;
  bulk_sd.size = 0; // we won't decompress

  SPDLOG_DEBUG("bulk_sd.disk_size: {:08X}", bulk_sd.disk_size);

  // offset the descriptor beginning
  if (pos > seg_offset)
  {
    const uint32_t offset_in_seg = reliable_numeric_cast<uint32_t>(pos - seg_offset);
    SPDLOG_DEBUG("offset'in bulk by {:08X}", offset_in_seg);
    bulk_sd.offset_in_archive += offset_in_seg;
    bulk_sd.disk_size -= offset_in_seg;
  }

  seg_offset += segit->disk_size;
  ++segit;

  SPDLOG_DEBUG("bulk_sd.disk_size: {:08X}", bulk_sd.disk_size);
    
  for (; segit != m_segment_descs.end(); ++segit)
  {
    const auto& sd = *segit;
    const bstreampos seg_end = seg_offset + sd.disk_size;

    SPDLOG_DEBUG("seg_end: {:08X}", seg_end);

    if (end <= seg_end || sd.offset_in_archive != bulk_sd.end_offset_in_archive())
    {
      break;
    }

    bulk_sd.disk_size += sd.disk_size;
    seg_offset += sd.disk_size;
    SPDLOG_DEBUG("bulk_sd.disk_size: {:08X}", bulk_sd.disk_size);
  }

  if (!m_archive->read_segment(bulk_sd, dst.subspan(0, bulk_sd.disk_size), false))
  {
    SPDLOG_ERROR("couldn't bulk read segments");
    return false;
  }

  m_pos += bulk_sd.disk_size;
  rsize = bulk_sd.disk_size;
  return true;
}

} // namespace redx

