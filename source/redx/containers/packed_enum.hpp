#pragma once
#include <type_traits>
#include <utility>

#include <redx/core.hpp>

namespace redx {

// to pack C enums

template <typename EnumT>
struct packed_enum
{
  using enum_type = EnumT;
  using underlying_type = std::underlying_type_t<EnumT>;

  packed_enum& operator=(const enum_type& value)
  {
    m_ut_value = static_cast<underlying_type>(value);
    return *this;
  }

  operator enum_type() const
  {
    return static_cast<enum_type>(m_ut_value);
  }

private:

  underlying_type m_ut_value;
};

} // namespace redx

