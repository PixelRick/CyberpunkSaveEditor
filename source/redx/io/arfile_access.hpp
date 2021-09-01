#pragma once
#include <redx/core.hpp>
#include <redx/radr/archive.hpp>
#include <redx/io/file_access.hpp>

namespace redx {

// this one is copyable..
struct arfile_access
  : file_access
{
  enum option : uint32_t
  {
    none = 0,
    //header_only = 1,
    warn_on_partial_buffer_reads = 2, // this is an option, because TFS needs to be able to do partial reads, but the lib/users shouldn't
    //minimize_buffering = 2, // skips buffering for whole segment or file reads
  };

  arfile_access() = default;
  ~arfile_access() override = default;

  arfile_access(const std::shared_ptr<radr::archive>& archive, uint32_t index, option options = option::none)
  {
    open(archive, index, options);
  }

  // in sequential_reading mode segments are released once fully read
  // otherwise loaded segments stay in memory (todo: limit the number of segments..)
  bool open(const std::shared_ptr<radr::archive>& archive, uint32_t index, option options = option::none);

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

  FORCE_INLINE bool is_open() const noexcept
  {
    return m_archive != nullptr;
  }

  bstreampos tell() const override
  {
    return m_pos;
  }

  std::streamsize size() const override
  {
    return static_cast<std::streamsize>(m_finfo.size);
  }

  bool seekpos(bstreampos pos) override
  {
    if (pos > 0 && static_cast<size_t>(pos) <= m_finfo.size)
    {
      m_pos = pos;
      return true;
    }
    return false;
  }

  FORCE_INLINE size_t buffered_size() const
  {
    return m_buffer.size();
  }

  bool buffer_all()
  {
    m_buffer_pos = 0;
    m_buffer.resize(m_finfo.size);

    if (!m_archive->read_file(m_findex, m_buffer))
    {
      m_buffer.clear();
      return false;
    }

    return true;
  }

  bool read_some(const std::span<char>& dst, size_t& rsize);

  bool read(char* dst, size_t count)
  {
    if (!is_open() || static_cast<std::streamoff>(m_pos) < 0)
    {
      return false;
    }

    const size_t dst_end = size_t(m_pos) + count;

    if (dst_end > m_finfo.size)
    {
      return false;
    }

    std::span<char> dstrem {dst, count};
    while (dstrem.size())
    {
      size_t rsize{};
      if (!read_some(dstrem, rsize))
      {
        return false;
      }
      dstrem = dstrem.subspan(rsize);
    }

    return true;
  }

protected:

  void reset()
  {
    m_archive.reset();
    m_findex = uint32_t(-1);
    m_options = option::none;
    m_finfo = radr::file_info();
    m_segment_descs.clear();
    m_buffer.clear();
    m_buffer_pos = 0;
    m_pos = bstreampos(-1);
  }

private:

  std::shared_ptr<const radr::archive> m_archive;
  uint32_t m_findex;
  option m_options = option::none;
  radr::file_info m_finfo;

  std::vector<radr::segment_descriptor> m_segment_descs;

  std::vector<char> m_buffer;
  bstreampos m_buffer_pos = 0;

  bstreampos m_pos = 0;
};

} // namespace redx

