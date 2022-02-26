#pragma once
#include <inttypes.h>
#include <string>
#include <spdlog/spdlog.h>

namespace redx::csav {

struct version
{
  friend bool operator==(const version& a, const version& b)
  {
    return (&a == &b) || (a.v1 == b.v1 && a.v2 == b.v2 && a.v3 == b.v3);
  }

  friend bool operator!=(const version& a, const version& b)
  {
    return !(a == b);
  }

  std::string string() const
  {
    return fmt::format("v{}-{}.{}{}", v1, v2, v3, (ps4w ? "(PS4W)" : ""));
  }

  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t uk0 = 0;
  uint32_t uk1 = 0;
  std::string suk;
  bool ps4w = false;
};

} // namespace redx::csav

