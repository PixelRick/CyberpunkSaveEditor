#pragma once
#include <filesystem>
#include <vector>
#include <shared_mutex>
#include <numeric>
#include <memory>

#include <redx/core.hpp>
#include <redx/io/file_access.hpp>
#include <redx/radr/radr_format.hpp>
#include <redx/radr/file_info.hpp>

namespace redx::radr {

using segment_descriptor = format::segment_descriptor;
using dependency = format::dependency;

//#define CACHE_FILE_SIZE

// read-only, standard thread-safety
// lookup isn't provided, a mgr should do it instead
struct archive
  : public std::enable_shared_from_this<archive>
{
  // same as radr::file_record without sha1 member
  struct file_record
    : format::file_record
  {
    file_record() = default;
  
    file_record(const format::file_record& record)
      : format::file_record(record)
    {}

#ifdef CACHE_FILE_SIZE
    size_t      disk_size = 0;
    size_t      file_size = 0;
#endif
  };

protected:
  
  struct create_tag {};

public:

  explicit archive(create_tag&&) {};

  ~archive() = default;

  archive(const archive&) = delete;
  archive& operator=(const archive&) = delete;

  bool open(const std::filesystem::path& p);
  
  // no close() because an archive is not meant to be closed!
  // it is shared and thus it is meant to wait for destruction.

  bool is_open() const
  {
    return m_freader && m_freader->is_open();
  }

  static std::shared_ptr<archive> create()
  {
    return std::make_shared<archive>(create_tag{});
  }

  inline const std::filesystem::path& path() const
  {
    return m_path;
  }

  inline const format::version& ver() const
  {
    return m_ver;
  }

  inline size_t size() const
  {
    return m_records.size();
  }

  file_info get_file_info(uint32_t index) const;

  bool read_segment(const segment_descriptor& sd, const std::span<char>& dst, bool decompress) const;

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

  const std::vector<segment_descriptor>& segments() const
  {
    return m_segments;
  }

  const std::vector<dependency>& dependencies() const
  {
    return m_dependencies;
  }

protected:

  archive() = default;

  bool read(size_t offset, const std::span<char>& dst) const;

private:

  std::filesystem::path                     m_path;
  format::version                           m_ver;

  std::vector<file_record>                  m_records;
  std::vector<segment_descriptor>           m_segments;
  std::vector<dependency>                   m_dependencies;

  mutable std::unique_ptr<std_file_access>  m_freader;
  mutable std::mutex                        m_freader_mtx;
};

} // namespace redx::radr

