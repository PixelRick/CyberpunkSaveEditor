#pragma once
#include <inttypes.h>
#include <vector>
#include <algorithm>

#include <redx/core/utils.hpp> // maybe these utils could go in a mini lib..

namespace dumper {

struct address_range
{
  uintptr_t beg = 0;
  uintptr_t end = 0;

  size_t size() const
  {
    return end - beg;
  }

  operator bool() const
  {
    return beg < end;
  }

  bool operator==(const address_range& rhs) const
  {
    return beg == rhs.beg && end == rhs.end;
  }
};

inline std::vector<uintptr_t> find_pattern_in(const address_range& range, std::wstring masked_pattern)
{
  std::vector<uintptr_t> ret;

  const char* it = reinterpret_cast<const char*>(range.beg);
  const char* end = reinterpret_cast<const char*>(range.end);
  
  while (1)
  {
    it = redx::sse2_strstr_masked(it, end - it, masked_pattern.data(), masked_pattern.size());
    if (!it)
      break;
    ret.push_back(reinterpret_cast<uintptr_t>(it));
    ++it;
  }

  return ret;
}

} // namespace dumper

