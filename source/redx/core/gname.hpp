#pragma once
#include "stringpool.hpp"
#include <nlohmann/json.hpp>
#include <redx/common/gstring.hpp>

namespace redx {

inline constexpr uint32_t gstring_name_category_tag = 'GNAM';

using gname         = typename gstring<gstring_name_category_tag>;
using literal_gname = typename literal_gstring<gstring_name_category_tag>;


constexpr literal_gname operator""_gn(const char* str, std::size_t len)
{
  return literal_gname{str};
}

} // namespace redx

