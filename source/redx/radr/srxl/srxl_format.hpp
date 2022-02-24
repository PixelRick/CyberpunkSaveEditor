#pragma once
#include <filesystem>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <numeric>

#include <redx/core.hpp>
#include <redx/io/bstream.hpp>
//#include <redx/serialization/stringvec.hpp>

namespace redx::radr::srxl::format {

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
    return magic == 'SRXL';
  }

  uint32_t  magic = 'SRXL';
  version   ver = {2}; // sorry fuzzo but I wanted something faster than a simple full paths list ^^
  uint32_t  mem_size;
  uint32_t  disk_size;
  uint32_t  svec_descs_size;
  uint32_t  svec_data_size;
};

static_assert(sizeof(header) == 0x18);

struct record
  : trivially_serializable<record>
{
  uint32_t  name_idx : 31;
  uint32_t  is_directory : 1;
  int32_t   parent_record_idx;
  fnv1a64_t hash;
};

//static_assert(sizeof(record) == 0x8);

constexpr int32_t root_idx = -1;

struct srxl
{
  srxl() = default;
  ~srxl() = default;

  bool serialize_in(ibstream& st, bool check_crc = true);
  bool serialize_out(obstream& st) const;

  inline std::string_view get_name(uint32_t idx) const
  {
    return "";//svec.at(idx);
  }

  //stringvec<true> svec;
  std::vector<std::string_view> svec;
  std::vector<record> records;
};

} // namespace redx::radr::srxl::format

