#pragma once
#include <inttypes.h>
#include <string>
#include <fmt/format.h>

namespace cp::csav {

struct csav_version
{
  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t uk0 = 0;
  uint32_t uk1 = 0;
  std::string suk;
  bool ps4w = false;

  std::string string() const
  {
    return fmt::format("v{}-{}.{}{}", v1, v2, v3, (ps4w ? "(PS4W)" : ""));
  }
};

inline bool operator==(const csav_version& a, const csav_version& b)
{
  return (&a == &b) || (a.v1 == b.v1 && a.v2 == b.v2 && a.v3 == b.v3);
}

inline bool operator!=(const csav_version& a, const csav_version& b)
{
  return !(a == b);
}

} // namespace cp::csav

