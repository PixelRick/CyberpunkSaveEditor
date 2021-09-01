#pragma once
#include <redx/radr/radr_format.hpp>

namespace redx::radr {

using file_id = format::file_id;

struct file_info
{
  file_id     id;
  file_time   time;
  size_t      disk_size = 0; // disk size (compressed)
  size_t      size      = 0; // file size (first segment uncompressed)
};

} // namespace redx::archive

