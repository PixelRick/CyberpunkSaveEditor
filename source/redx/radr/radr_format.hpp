#pragma once
#include <vector>
#include <numeric>

#include <redx/core.hpp>
#include <redx/io/bstream.hpp>

namespace redx::radr::format {

using file_id = redx::path_id;

struct version
{
  friend bool operator==(const version& a, const version& b)
  {
    return (&a == &b) || (a.v1 == b.v1);
  }
  
  friend bool operator!=(const version& a, const version& b)
  {
    return !(a == b);
  }

  std::string string() const
  {
    return fmt::format("v{}", v1);
  }

  uint32_t v1 = 0;
};


struct header
  : trivially_serializable<header>
{
  bool is_magic_ok() const
  {
    return magic == 'RADR';
  }

  uint32_t magic = 'RADR';
  version  ver = {};
  uint64_t metadata_offset = 0;
  uint32_t metadata_size = 0;
  uint64_t uk_offset = 0;
  uint32_t uk_size = 0;
  uint64_t fullsize = 0;
};


struct file_record
  : trivially_serializable<file_record>
{
  file_id     fid;
  file_time   ftime;
  uint32_t    inl_buffer_segs_cnt;
  u32range    segs_irange;
  u32range    deps_irange;
  sha1_digest sha1; // interesting to have..
};

static_assert(sizeof(sha1_digest) == 0x14);
static_assert(sizeof(file_record) == 0x38);


struct segment_descriptor
  : trivially_serializable<segment_descriptor>
{
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
  : trivially_serializable<dependency>
{
  uint64_t hpath;
};

static_assert(sizeof(dependency) == 0x8);


struct metadata_trampoline
  : trivially_serializable<metadata_trampoline>
{
  uint32_t tbls_offset = 8;
  uint32_t tbls_size = 0;
};

#pragma pack(push, 4)
struct metadata_tbls_header
  : trivially_serializable<metadata_tbls_header>
{
  uint64_t crc = 0;
  uint32_t files_cnt = 0;
  uint32_t segments_cnt = 0;
  uint32_t dependencies_cnt = 0;
};
#pragma pack(pop)

static_assert(sizeof(metadata_tbls_header) == 0x14);

struct metadata
{
  metadata() = default;
  ~metadata() = default;

  bool serialize_in(ibstream& st, bool check_crc = true);
  bool serialize_out(obstream& st) const;

  uint64_t compute_tbls_crc64() const;

  std::vector<file_record>        records;
  std::vector<segment_descriptor> segments;
  std::vector<dependency>         dependencies;
};

inline ibstream& operator>>(ibstream& st, metadata& x)
{
  x.serialize_in(st, true);
  return st;
}

inline obstream& operator<<(obstream& st, const metadata& x)
{
  x.serialize_out(st);
  return st;
}

} // namespace redx::radr::format

template <>
struct std::hash<redx::radr::format::dependency>
{
  std::size_t operator()(const redx::radr::format::dependency& k) const noexcept
  {
    return k.hpath;
  }
};

