#pragma once
#include <inttypes.h>
#include <vector>
#include <algorithm>

#include <redx/common/utils.hpp> // maybe these utils could go in a mini lib..

namespace dumper {

struct address_range
{
  uintptr_t start = 0;
  uintptr_t end = 0;

  size_t size() const
  {
    return end - start;
  }

  operator bool() const
  {
    return start < end;
  }

  bool operator==(const address_range& rhs) const
  {
    return start == rhs.start && end == rhs.end;
  }
};

inline std::vector<uintptr_t> find_pattern_in(const address_range& range, std::string pattern, std::string mask = "")
{
  const uint8_t* needle = reinterpret_cast<unsigned char*>(pattern.data());
  std::vector<uintptr_t> ret = redx::sse2_strstr_masked(range.start, range.size(), needle, pattern.size(), mask.c_str());
  std::transform(std::begin(ret), std::end(ret), std::begin(ret), [](uintptr_t x) { return x; });
  return ret;
}

} // namespace dumper

