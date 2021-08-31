#pragma once
#include <filesystem>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <numeric>
#include <memory>

#include <redx/core.hpp>
#include <redx/archive/radr.hpp>
#include <redx/os/file_reader.hpp>

namespace redx {

//#define CACHE_FILE_SIZE

// read-only, standard thread-safety
struct archive
  : public std::enable_shared_from_this<archive>
{
  using file_id = radr::file_id;

  struct file_info
  {
    file_id     id;
    file_time   time;
    size_t      disk_size = 0; // disk size (compressed)
    size_t      size      = 0; // file size (first segment uncompressed)
  };
  
  // same as radr::file_record without sha1 member
  struct file_record
  {
    file_record() = default;
  
    file_record(const redx::radr::file_record& record)
      : fid(record.fid)
      , ftime(record.ftime)
      , segs_irange(record.segs_irange)
      , deps_irange(record.deps_irange)
      , inl_buffer_segs_cnt(record.inl_buffer_segs_cnt)
    {}
  
    file_id     fid;
    file_time   ftime;
    u32range    segs_irange;
    u32range    deps_irange;
    uint32_t    inl_buffer_segs_cnt = 0;
    uint32_t    unused[1];
#ifdef CACHE_FILE_SIZE
    size_t      disk_size = 0;
    size_t      file_size = 0;
#endif
  };

  // file readers (stream, w2rc, ..) know what to do with it
  struct file_handle
  {
    file_handle() = default;
  
    inline bool is_valid() const
    {
      const bool ret = (m_archive != nullptr);
      if (ret)
      {
        assert(m_file_index < m_archive->size());
      }
      return ret;
    }
  
    inline const std::shared_ptr<const archive>& source_archive() const
    {
      return m_archive;
    }
  
    inline uint32_t file_index() const
    {
      return m_file_index;
    }

  protected:
  
    friend archive;
  
    file_handle(const std::shared_ptr<const archive>& archive, uint32_t file_index)
      : m_archive(archive), m_file_index(file_index) {}
  
  private:
  
    std::shared_ptr<const archive> m_archive;
    uint32_t m_file_index = 0;
  };

protected:
  
  struct create_tag {};

public:

  archive(const std::filesystem::path& p, radr::metadata&& md, os::file_reader&& freader, create_tag&&);

  ~archive() = default;

  archive(const archive&) = delete;
  archive& operator=(const archive&) = delete;

  static std::shared_ptr<archive> load(const std::filesystem::path& path);

  inline const std::filesystem::path& path() const
  {
    return m_path;
  }

  inline const redx::radr::version& ver() const
  {
    return m_ver;
  }

  inline size_t size() const
  {
    return m_records.size();
  }

  inline file_handle get_file_handle(uint32_t index) const
  {
    if (index >= m_records.size())
    {
      SPDLOG_CRITICAL("index out of range");
      DEBUG_BREAK();
    }

    return {shared_from_this(), index};
  }

  file_info get_file_info(uint32_t index) const;

  bool read_segment(const redx::radr::segment_descriptor& sd, const std::span<char>& dst, bool decompress) const;

  inline bool is_valid_segments_irange(const u32range& segs_irange) const
  {
    return segs_irange.end() <= m_segments.size();
  }

  bool read_segments_raw(u32range segs_irange, const std::span<char>& dst) const;

  bool read_file(uint32_t idx, const std::span<char>& dst) const;

  const std::vector<file_record>& records() const
  {
    return m_records;
  }

  const std::vector<redx::radr::segment_descriptor>& segments() const
  {
    return m_segments;
  }

  const std::vector<redx::radr::dependency>& dependencies() const
  {
    return m_dependencies;
  }

protected:

  archive() = default;

  bool read(size_t offset, const std::span<char>& dst) const;

private:

  std::filesystem::path                     m_path;
  redx::radr::version                         m_ver;

  std::vector<file_record>                  m_records;
  std::vector<redx::radr::segment_descriptor> m_segments;
  std::vector<redx::radr::dependency>         m_dependencies;

  mutable os::file_reader                   m_freader;
  mutable std::mutex                        m_freader_mtx;
};

} // namespace redx

