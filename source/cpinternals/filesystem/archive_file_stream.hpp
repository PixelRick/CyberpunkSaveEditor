#pragma once
#include <fstream>
#include <filesystem>
#include <cpinternals/common.hpp>
#include <cpinternals/filesystem/archive.hpp>

namespace cp {

// not thread safe
struct archive_file_stream
  : streambase
{
  enum option : uint32_t
  {
    none = 0,
    //minimize_buffering = 1, // skips buffering for whole segment or file reads
  };

  archive_file_stream() = default;
  ~archive_file_stream() override = default;

  archive_file_stream(const archive::file_handle& handle, option options = option::none)
  {
    open(handle, options);
  }

  bool is_reader() const override
  {
    return true;
  }

  // in sequential_reading mode segments are released once fully read
  // otherwise loaded segments stay in memory (todo: limit the number of segments..)
  bool open(const archive::file_handle& handle, option options = option::none)
  {
    if (is_open())
    {
      SPDLOG_ERROR("already open");
      return false;
    }

    if (!handle.is_valid())
    {
      SPDLOG_ERROR("invalid handle");
      return false;
    }

    clear_error();

    m_archive = handle.source_archive();
    m_findex = handle.file_index();
    m_options = options;
    m_finfo = m_archive->get_file_info(m_findex);

    const auto& frec = m_archive->records()[m_findex];
    auto segs = frec.segs_irange.slice(m_archive->segments());

    m_segment_descs = std::vector<radr::segment_descriptor>(segs.begin(), segs.end());

    // here we could merge descriptors to tweak the buffering if desired
    // the game seems to buffer "inline buffers" with the first segment

    return true;
  }

  bool close()
  {
    if (!is_open())
    {
      SPDLOG_ERROR("was not open");
      return false;
    }

    reset();
    return true;
  }

  bool is_open() const
  {
    return m_archive != nullptr;
  }

  pos_type tell() const override
  {
    return m_pos;
  }

  streambase& seek(pos_type pos) override
  {
    seek(pos, beg);
    return *this;
  }

  streambase& seek(off_type off, seekdir dir) override
  {
    clear_error();

    switch (dir)
    {
      case beg:
        m_pos = off;
        break;
      case cur:
        m_pos += off;
        break;
      case end:
        m_pos = size() + off;
        break;
    }

    if (m_pos < 0 || size_t(m_pos) >= size())
    {
      set_error("invalid seek pos");
    }

    return *this;
  }

  streambase& serialize_bytes(void* data, size_t size) override
  {
    char* pc = reinterpret_cast<char*>(data);

    read(std::span<char>(pc, pc + size));

    return *this;
  }

  size_t size() const
  {
    return m_finfo.size;
  }

  size_t buffered_size() const
  {
    return m_buffer.size();
  }

  bool buffer_all()
  {
    m_buffer_pos = 0;
    m_buffer.resize(size());

    if (!m_archive->read_file(m_findex, m_buffer))
    {
      m_buffer.clear();
      return false;
    }

    return true;
  }

  // returns the count of bytes read
  // returns 0 on error (and sets internal error)
  size_t read_some(const std::span<char>& dst)
  {
    if (!check_state())
    {
      return 0;
    }

    const size_t pos = size_t(m_pos);
    const size_t end = pos + dst.size();

    SPDLOG_DEBUG("pos:{:08X} end:{:08X}", pos, end);

    if (end > size())
    {
      set_error("out-of-bounds read");
      return 0;
    }

    // try read from buffer
    
    if (pos >= m_buffer_pos && end <= m_buffer_pos + m_buffer.size())
    {
      const size_t read_size = end - pos;

      auto beg = m_buffer.begin() + (pos - m_buffer_pos);
      std::copy(beg, beg + read_size, dst.begin());

      SPDLOG_DEBUG("read buffered");
      m_pos += read_size;
      return read_size;
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
          set_error("couldn't read and decompress first segment");
          return 0;
        }

        SPDLOG_DEBUG("read full sd0");
        m_pos += sd0.size;
        return sd0.size;
      }
      else
      {
        m_buffer_pos = 0;
        m_buffer.resize(sd0.size);
        if (!m_archive->read_segment(sd0, m_buffer, true))
        {
          m_buffer.clear();
          set_error("couldn't read and decompress first segment");
          return 0;
        }

        const size_t read_end = std::min(size_t(sd0.size), end);
        const size_t read_size = read_end - pos;

        auto beg = m_buffer.begin() + pos;
        std::copy(beg, beg + read_size, dst.begin());

        SPDLOG_DEBUG("read partial sd0");
        m_pos += read_size;
        return read_size;
      }
    }

    // search segment for pos
    
    auto segit = m_segment_descs.begin() + 1;
    size_t seg_offset = sd0.size;

    for (; segit != m_segment_descs.end(); ++segit)
    {
      const size_t seg_end = seg_offset + segit->disk_size;

      if (pos < seg_end)
      {
        break;
      }

      seg_offset = seg_end;
    }

    if (segit == m_segment_descs.end())
    {
      set_error("logic error");
      return 0;
    }

    SPDLOG_DEBUG(
      "seg_offset:{:08X} seg_end:{:08X}",
      seg_offset, seg_offset + segit->disk_size);

    // do a buffered read if we can't skip/bulk
    if (end <= seg_offset + segit->disk_size) // read is limited to the found segment
    {
      if (pos != seg_offset || end < seg_offset + segit->disk_size) // read doesn't span the full segment
      {
        const auto& sd = *segit;

        // buffer segment
        m_buffer_pos = seg_offset;
        m_buffer.resize(sd.disk_size);
        if (!m_archive->read_segment(sd, m_buffer, false))
        {
          m_buffer.clear();
          set_error(fmt::format("couldn't read segment"));
          return 0;
        }

        const size_t read_end = end;
        const size_t read_size = read_end - pos;

        assert(read_end <= seg_offset + sd.disk_size);

        auto beg = m_buffer.begin() + (pos - seg_offset);
        std::copy(beg, beg + read_size, dst.begin());

        SPDLOG_DEBUG("read {:08X} bytes in buffered seg", read_size);
        m_pos += read_size;
        return read_size;
      }
    }

    // otherwise try bulk read

    radr::segment_descriptor bulk_sd = *segit;
    bulk_sd.size = 0; // we won't decompress

    SPDLOG_DEBUG("bulk_sd.disk_size: {:08X}", bulk_sd.disk_size);

    // offset the descriptor beginning
    if (pos > seg_offset)
    {
      const uint32_t offset_in_seg = static_cast<uint32_t>(pos - seg_offset);
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
      const size_t seg_end = seg_offset + sd.disk_size;

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
      set_error(fmt::format("couldn't bulk read segments"));
      return 0;
    }

    m_pos += bulk_sd.disk_size;
    return bulk_sd.disk_size;
  }

  bool read(const std::span<char>& dst)
  {
    if (!check_state())
    {
      return 0;
    }

    const size_t dst_end = size_t(m_pos) + dst.size();

    if (dst_end > size())
    {
      set_error("out-of-bounds read");
      return false;
    }

    std::span<char> dstrem = dst;

    while (dstrem.size())
    {
      size_t cnt = read_some(dstrem);
      if (cnt == 0)
      {
        return false;
      }
      dstrem = dstrem.subspan(cnt);
    }

    return true;
  }

protected:

  bool check_state()
  {
    if (has_error())
    {
      return false;
    }

    if (m_pos < 0)
    {
      set_error("m_pos < 0");
      return false;
    }

    if (!is_open())
    {
      set_error("no opened file");
      return false;
    }

    return true;
  }

  void reset()
  {
    *this = archive_file_stream();
  }

private:

  std::shared_ptr<const archive> m_archive;
  uint32_t m_findex;
  option m_options = option::none;
  archive::file_info m_finfo;

  std::vector<radr::segment_descriptor> m_segment_descs;

  std::vector<char> m_buffer;
  size_t m_buffer_pos = 0;

  pos_type m_pos = 0;
};

} // namespace cp

