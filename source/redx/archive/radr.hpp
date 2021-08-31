#pragma once
#include <filesystem>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <numeric>

#include <cpinternals/common.hpp>

namespace cp::radr {

using file_id = cp::path_id;

struct file_record
{
  friend streambase& operator<<(streambase& st, file_record& x)
  {
    return st.serialize_pod_raw(x);
  }

  file_id     fid;
  file_time   ftime;
  uint32_t    inl_buffer_segs_cnt;
  u32range    segs_irange;
  u32range    deps_irange;
  sha1_digest sha1;
};

static_assert(sizeof(sha1_digest) == 0x14);
static_assert(sizeof(file_record) == 0x38);

struct segment_descriptor
{
  friend streambase& operator<<(streambase& st, segment_descriptor& x)
  {
    return st.serialize_pod_raw(x);
  }

  inline bool is_segment_compressed() const
  {
    return size != disk_size;
  }

  inline uint64_t end_offset_in_archive() const
  {
    return offset_in_archive + disk_size;
  }

  uint64_t offset_in_archive = 0; // in archive file
  uint32_t disk_size = 0;
  uint32_t size = 0;
};

static_assert(sizeof(segment_descriptor) == 0x10);


struct dependency
{
  friend streambase& operator<<(streambase& st, dependency& x)
  {
    return st.serialize_pod_raw(x);
  }

  uint64_t hpath;
};

static_assert(sizeof(dependency) == 0x8);


struct version
{
  uint32_t v1 = 0;

  std::string string() const
  {
    return fmt::format("v{}", v1);
  }

  friend bool operator==(const version& a, const version& b)
  {
    return (&a == &b) || (a.v1 == b.v1);
  }
  
  friend bool operator!=(const version& a, const version& b)
  {
    return !(a == b);
  }
};


struct header
{
  uint32_t magic = 'RADR';
  version  ver{};
  uint64_t metadata_offset = 0;
  uint32_t metadata_size = 0;
  uint64_t uk_offset = 0;
  uint32_t uk_size = 0;
  uint64_t fullsize = 0;

  bool is_magic_ok() const
  {
    return magic == 'RADR';
  }
};


struct metadata
{
  metadata() = default;
  ~metadata() = default;

  void serialize(streambase& st, bool check_crc = true);

  uint64_t compute_tbls_crc64();

  std::vector<file_record>        records;
  std::vector<segment_descriptor> segments;
  std::vector<dependency>         dependencies;
};

} // namespace cp::radr


namespace std {

template <>
struct hash<cp::radr::dependency>
{
  std::size_t operator()(const cp::radr::dependency& k) const noexcept
  {
    return k.hpath;
  }
};

} // namespace std

