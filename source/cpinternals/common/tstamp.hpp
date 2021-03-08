#pragma once
#include <inttypes.h>
#include <chrono>

namespace cp {

using clock = std::chrono::high_resolution_clock;
using time_point = clock::time_point;
using time_stamp = uint64_t;

inline time_point tstamp_to_tpoint(time_stamp ts)
{
  return time_point(std::chrono::milliseconds(ts));
}

inline time_stamp tpoint_to_tstamp(time_point tp)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

} // namespace cp

