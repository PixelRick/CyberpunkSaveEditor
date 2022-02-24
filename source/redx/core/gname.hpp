#pragma once
#include <redx/core/platform.hpp>

#include <nlohmann/json.hpp>

#include <redx/core/stringpool.hpp>
#include <redx/core/gstring.hpp>

namespace redx {

inline constexpr uint32_t gstring_name_category_tag = 'GNAM';

using gname = gstring<gstring_name_category_tag>;

namespace literals {

using literal_gname_helper = literal_gstring_helper<gstring_name_category_tag>;

#if __cpp_nontype_template_args >= 201911 && !defined(__INTELLISENSE__)

// todo: finish that when c++20 finally works in msvc..

template <literal_gname_helper::builder Builder>
constexpr literal_gname_helper::singleton<Builder>::gstring_builder operator""_gndef()
{
  return literal_gname_helper::singleton<Builder>::gstring_builder();
}

//template <literal_gname_helper::builder Builder>
//constexpr literal_gname_helper::singleton<Builder>::gstrid_builder operator""_gndef_id()
//{
//  return literal_gname_helper::singleton<Builder>::gstrid_builder();
//}

#else

// does register the name
constexpr literal_gname_helper::gstring_builder operator""_gndef(const char* s, std::size_t)
{
  return literal_gname_helper::gstring_builder(s);
}

//constexpr literal_gname_helper::gstrid_builder operator""_gnid(const char* s, std::size_t)
//{
//  return literal_gname_helper::gstrid_builder(s);
//}

#endif

inline constexpr auto gn_test1 = literal_gname_helper::builder("test1");
inline constexpr auto gn_test2 = "test2"_gndef;

} // namespace literals

using literals::operator""_gndef;

} // namespace redx
