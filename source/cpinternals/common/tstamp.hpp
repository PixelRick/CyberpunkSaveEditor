#pragma once
#include <inttypes.h>
#include <chrono>

namespace cp {

using clock = std::chrono::high_resolution_clock;
using time_point = clock::time_point;

using hectonanoseconds = std::chrono::duration<int64_t, std::ratio_multiply<std::hecto, std::nano>>;

struct time_stamp
{
  time_stamp() = default;

  explicit time_stamp(time_point tp) noexcept
    : ms_since_unix_epoch(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count()) {}

  explicit time_stamp(uint64_t ms_since_unix_epoch) noexcept
    : ms_since_unix_epoch(ms_since_unix_epoch) {}

  time_stamp& operator=(time_point tp) noexcept
  {
     *this = time_stamp(tp);
     return *this;
  }

  time_stamp& operator=(uint64_t ms_since_unix_epoch) noexcept
  {
     this->ms_since_unix_epoch = ms_since_unix_epoch;
     return *this;
  }

  bool operator<(const time_stamp& rhs) const
  {
    return ms_since_unix_epoch < rhs.ms_since_unix_epoch;
  }

  bool operator==(const time_stamp& rhs) const
  {
    return ms_since_unix_epoch == rhs.ms_since_unix_epoch;
  }

  bool operator!=(const time_stamp& rhs) const
  {
    return ms_since_unix_epoch != rhs.ms_since_unix_epoch;
  }

  inline operator time_point() const noexcept
  {
    return time_point(std::chrono::milliseconds(ms_since_unix_epoch));
  }

  uint64_t ms_since_unix_epoch = 0;
};

struct file_time
{
  file_time() = default;

  explicit file_time(time_point tp) noexcept
    : hns_since_win_epoch(std::chrono::duration_cast<hectonanoseconds>(tp.time_since_epoch()).count() + 116444736000000000) {}

  explicit file_time(uint64_t hns_since_win_epoch) noexcept
    : hns_since_win_epoch(hns_since_win_epoch) {}

  file_time& operator=(time_point tp) noexcept
  {
     *this = file_time(tp);
     return *this;
  }

  file_time& operator=(uint64_t hns_since_win_epoch) noexcept
  {
     this->hns_since_win_epoch = hns_since_win_epoch;
     return *this;
  }

  bool operator<(const file_time& rhs) const
  {
    return hns_since_win_epoch < rhs.hns_since_win_epoch;
  }

  bool operator==(const file_time& rhs) const
  {
    return hns_since_win_epoch == rhs.hns_since_win_epoch;
  }

  bool operator!=(const file_time& rhs) const
  {
    return hns_since_win_epoch != rhs.hns_since_win_epoch;
  }

  inline operator time_point() const noexcept
  {
    return time_point(hectonanoseconds(hns_since_win_epoch - 116444736000000000));
  }

  uint64_t hns_since_win_epoch = 0;
};

} // namespace cp

