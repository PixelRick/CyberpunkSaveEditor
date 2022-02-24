#pragma once
#include <filesystem>
#include <fstream>
#include <memory>

#include <redx/core.hpp>
#include <redx/io/bstream.hpp>
#include <redx/io/file_access.hpp>

namespace redx {

template <typename CRTP>
struct std_file_bstream_mixin
{
  bool is_open() const
  {
    auto crtp = static_cast<const CRTP*>(this);
    return crtp->m_buf.is_open();
  }

  void close()
  {
    auto crtp = static_cast<CRTP*>(this);
    if (!crtp->m_buf.close())
    {
      crtp->set_fail_with_msg("failed to close file");
    }
  }
};

struct std_file_ibstream
  : ibstream, std_file_bstream_mixin<std_file_ibstream>
{
  using bstreambuf_type = std::filebuf;
  using mixin_type = std_file_bstream_mixin<std_file_ibstream>;
  friend mixin_type;

  std_file_ibstream()
    : ibstream(m_buf) {}

  explicit std_file_ibstream(const std::filesystem::path& p, std::ios::openmode mode = std::ios::in)
    : ibstream(m_buf)
  {
    open(p, mode);
  }

  bstreambuf_type* open(const std::filesystem::path& p, std::ios::openmode mode = std::ios::in)
  {
    return m_buf.open(p, mode | std::ios::binary);
  }

protected:

  bstreambuf_type m_buf;
};

struct std_file_obstream
  : obstream, std_file_bstream_mixin<std_file_obstream>
{
  using bstreambuf_type = std::filebuf;
  using mixin_type = std_file_bstream_mixin<std_file_obstream>;
  friend mixin_type;

  std_file_obstream()
    : obstream(m_buf) {}

  explicit std_file_obstream(const std::filesystem::path& p, std::ios::openmode mode = std::ios::out)
    : obstream(m_buf)
  {
    open(p, mode);
  }

  bstreambuf_type* open(const std::filesystem::path& p, std::ios::openmode mode = std::ios::out)
  {
    return m_buf.open(p, mode | std::ios::binary);
  }

protected:

  bstreambuf_type m_buf;
};


// let's use std::filebuf for now !!!
// why ?
// e.g.: one does not simply buffer cr2w file access
//  -> better extract the first segment in memory first..

// file bstreambuf
//struct file_bstreambuf
//  : std::streambuf
//{
//  static constexpr size_t sector_size = 4096;
//
//  file_bstreambuf() noexcept
//  {
//    m_buf_next_size = win_osfile::get_best_buffer_size();
//  }
//
//  win_osfile_bstreambuf(const std::filesystem::path& p, std::ios::openmode mode)
//    : win_osfile_bstreambuf()
//  {
//    open(p, mode);
//  }
//
//  ~win_osfile_bstreambuf() override
//  {
//    close();
//  }
//
//  const std::filesystem::path& path() const override
//  {
//    return m_file.path();
//  }
//
//  bool open(const std::filesystem::path& p, std::ios::openmode mode) override
//  {
//    if (m_file.open(p, mode))
//    {
//      m_can_read = m_file.can_read();
//      m_can_write = m_file.can_write();
//      return true;
//    }
//
//    return false;
//  }
//
//  bool close() override
//  {
//    if (m_file.is_open() && flush())
//    {
//      reset_buffer_pointers();
//      m_can_read = false;
//      m_can_write = false;
//      m_cur_pos = size_t(-1);
//      return m_file.close();
//    }
//
//    return false;
//  }
//
//  bool is_open() const override
//  {
//    return m_file.is_open();
//  }
//
//  // supports multiples of 4096
//  streamsize set_buffer_size(streamsize size)
//  {
//    if (size == streamsize(-1))
//    {
//      m_buf_next_size = win_osfile::get_best_buffer_size();
//    }
//    else if (size > 0)
//    {
//      //m_buf_next_size = 1ull << (32 - clz(size - 1));
//      //m_buf_next_size = std::max(m_buf_next_size, sector_size);
//      m_buf_next_size = align_up(size, sector_size);
//    }
//    else
//    {
//      m_buf_next_size = 0;
//    }
//
//    return m_buf_next_size;
//  }
//
//  streamsize get_buffer_size() const override
//  {
//    return m_buf_next_size;
//  }
//
//  streamsize size() const override
//  {
//    return m_file.size(); // returns -1 when not open
//  }
//
//  streampos tell() const override
//  {
//    return m_cur_pos;
//  }
//
//  // adjusts the cursor position and sync the buffer
//  bool seekpos(streampos pos) override
//  {
//    if (m_can_write)
//    {
//      m_wend = std::max(m_wend, m_bcur);
//    }
//
//    if (m_can_read)
//    {
//      m_rend = std::max(m_rend, m_bcur);
//    }
//
//    // check if pos is part of the buffered area
//    const char* max_bcur = std::max(m_rend, m_wend);
//    const size_t max_bcur_pos = m_buf_pos + (max_bcur - m_bbeg);
//    if (pos >= m_buf_pos && pos < max_bcur_pos) // excludes last position
//    {
//      m_bcur = m_bbeg + (pos - m_buf_pos);
//      m_cur_pos = pos;
//      return true;
//    }
//
//    // otherwise try to set file pointer
//
//    if (!m_file.is_open())
//    {
//      return false;
//    }
//
//    if (!m_file.seekpos(pos))
//    {
//      // in case everything fails (past eof for instance..)
//      m_cur_pos = m_file.tell();
//      return false;
//    }
//
//    m_cur_pos = pos;
//
//    // no need to flush right now
//    // read/write should do it
//
//    return true;
//  }
//
//  bool seekoff(streamoff off, ios::seekdir dir) override
//  {
//    if (!is_open())
//    {
//      return false;
//    }
//
//    int64_t new_pos{};
//
//    switch (dir)
//    {
//      case ios::seekdir::cur:
//        new_pos = static_cast<int64_t>(m_cur_pos) + off;
//        break;
//      case ios::seekdir::end:
//        new_pos = m_file.size() + off;
//        break;
//      case ios::seekdir::beg:
//      default:
//        new_pos = off;
//        break;
//    }
//
//    if (new_pos >= 0)
//    {
//      return seekpos(streampos(new_pos));
//    }
//
//    return false;
//  }
//
//  bool read(char* dst, streamsize count) override
//  {
//    DWORD rsize{};
//    return m_file.read(dst, count, rsize) && rsize == count;
//
//    if (count == 0)
//      return true;
//
//    // try fast read from buffer
//    if (m_bcur < m_rend)
//    {
//      const size_t avail = m_rend - m_bcur;
//      const size_t rsize = std::min(avail, count);
//      std::memcpy(dst, m_bcur, rsize);
//      m_cur_pos += rsize;
//      m_bcur += rsize;
//      count -= rsize;
//
//      if (count == 0)
//        return true;
//
//      dst += rsize;
//    }
//
//    if (!m_can_read)
//      return false;
//
//    if (m_buf_size && !flush())
//    {
//      return false;
//    }
//
//    update_buffer_size();
//
//    // unbuffered
//    if (m_buf_size == 0 || (count > (m_buf_size >> 2)))
//    {
//      if (!m_file.seekpos(m_cur_pos))
//      {
//        return false;
//      }
//
//      DWORD rsize{};
//      bool success = m_file.read(dst, count, rsize);
//      m_cur_pos += rsize;
//      return success && (count == rsize);
//    }
//
//    // from here we have (count <= m_buf_size / 4)
//    // so we'll be able to copy all remaining bytes from buffer
//    // except on EOF !
//
//    if (!buffer_current())
//    {
//      return false;
//    }
//
//    const size_t avail = m_rend - m_bcur;
//    if (avail < count)
//    {
//      SPDLOG_ERROR("EOF");
//      return false;
//    }
//
//    std::memcpy(dst, m_bcur, count);
//    m_cur_pos += count;
//    m_bcur += count;
//
//    return true;
//  }
//
//  bool write(const char* src, streamsize count) override
//  {
//    if (count == 0)
//      return true;
//
//    if (!m_can_write)
//      return false;
//
//    // try fast write to buffer
//    if (m_bcur < m_bend)
//    {
//      const size_t avail = m_bend - m_bcur;
//      const size_t rsize = std::min(avail, count);
//      std::memcpy(m_bcur, src, rsize);
//      m_cur_pos += rsize;
//      m_bcur += rsize;
//      m_pending_flush = true;
//
//      src += rsize;
//      count -= rsize;
//
//      // don't adjust rend/wend here !
//      // because only seeking would let you re-read pending writes
//    }
//
//    if (count == 0)
//      return true;
//
//    if (m_buf_size && !flush())
//    {
//      return false;
//    }
//
//    update_buffer_size();
//
//    // unbuffered
//    if (m_buf_size == 0 || (count > (m_buf_size >> 2)))
//    {
//      if (!m_file.seekpos(m_cur_pos))
//      {
//        return false;
//      }
//
//      DWORD wsize{};
//      bool success = m_file.write(src, count, wsize);
//      m_cur_pos += wsize;
//      return success && (count == wsize);
//    }
//
//    // from here we have (count <= m_buf_size / 4)
//    // so we'll be able to copy all remaining bytes to the buffer
//
//    if (!buffer_current())
//    {
//      return false;
//    }
//
//    const size_t avail = m_bend - m_bcur;
//    if (avail < count)
//    {
//      SPDLOG_CRITICAL("bad logic");
//      return false;
//    }
//
//    std::memcpy(m_bcur, src, count);
//    m_cur_pos += count;
//    m_bcur += count;
//    m_pending_flush = true;
//
//    return true;
//  }
//
//  bool flush() override
//  {
//    if (m_pending_flush)
//    {
//      REDX_ASSERT(is_open());
//      REDX_ASSERT(m_can_write);
//
//      if (!m_file.seekpos(m_buf_pos))
//      {
//        return false;
//      }
//
//      // m_wend is updated only on seek
//      // so let's get the real end of written-to area
//      char* wend = std::max(m_bcur, m_wend);
//      DWORD count = static_cast<DWORD>(wend - m_bbeg);
//
//      DWORD wsize{};
//      if (!m_file.write(m_bbeg, count, wsize))
//      {
//        return false;
//      }
//
//      if (wsize != count)
//      {
//        SPDLOG_ERROR("under-write on flush");
//        return false;
//      }
//
//      m_wend = m_bbeg; // reset end of written-to area
//      m_pending_flush = false;
//    }
//
//    return true;
//  }
//
//protected:
//
//  // assumes flush() has already been done
//  // it doesn't update the buffer size
//  bool buffer_current()
//  {
//    REDX_ASSERT(is_open());
//    REDX_ASSERT(!m_pending_flush);
//    REDX_ASSERT(m_wend == m_bbeg);
//
//    if (m_buf_size == 0)
//      return true;
//
//    if (!m_file.seekpos(m_cur_pos))
//    {
//      return false;
//    }
//
//    DWORD rsize = 0;
//    if (m_can_read && !m_file.read(m_buf.get(), m_buf_size, rsize))
//    {
//      return false;
//    }
//
//    m_buf_pos = m_cur_pos;
//    m_bcur = m_bbeg;
//    m_bend = m_bbeg + m_buf_size;
//    m_rend = m_bbeg + rsize;
//
//    return true;
//  }
//
//  // defines read and write areas as zero-sized
//  inline void reset_buffer_pointers()
//  {
//    m_bend = m_bbeg;
//    m_bcur = m_bbeg;
//    m_rend = m_bbeg;
//    m_wend = m_bbeg;
//  }
//
//  // reallocate the buffer if not already done or has wrong size
//  // define read and write area as zero-sized
//  // >>> don't forget to flush before doing that <<<
//  inline void update_buffer_size()
//  {
//    if (m_buf_next_size != m_buf_size)
//    {
//      m_buf_size = m_buf_next_size;
//      m_buf.reset((m_buf_size > 0) ? new char[m_buf_next_size] : nullptr);
//      m_bbeg = m_buf.get();
//      reset_buffer_pointers();
//    }
//  }
//
//private:
//
//  std::shared_ptr<file_access> m_file;
//
//  std::unique_ptr<char[]> m_buf;
//  size_t m_buf_size = 0;
//  size_t m_buf_next_size = sector_size * 2;
//
//  // buffered read/write is faster if pointers are ready
//  char* m_bbeg = nullptr;
//  char* m_bcur = nullptr;
//  char* m_bend = nullptr; // end of usable buffer area (equals bbeg if buffer isn't synced)
//  // these are only updated when seeking
//  char* m_rend = nullptr; // end of readable buffer area (< bend if EOF, and equals bbeg if not open in read mode)
//  char* m_wend = nullptr; // end of written-to buffer area (or EOF in read-write mode)
//
//  size_t m_buf_pos = 0;
//  size_t m_cur_pos = 0;
//
//  bool m_can_read = false;
//  bool m_can_write = false;
//  bool m_pending_flush = false;
//};
//


} // namespace redx

