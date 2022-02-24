#pragma once
#include <array>

#include <redx/core.hpp>

namespace redx {

template <typename T, size_t Size>
using array = std::array<T, Size>;


template <typename T>
struct is_array
  : std::false_type {};

template <typename T, size_t N>
struct is_array<T[N]>
  : std::true_type {};

template <typename T, size_t N>
struct is_array<redx::array<T, N>>
  : std::true_type {};

template <typename T>
inline constexpr bool is_array_v = is_array<T>::value;


template <typename T>
struct array_traits_t;

template <typename T, size_t N>
struct array_traits_t<T[N]>
{
  using element_type = remove_cvref_t<T>;
  static constexpr size_t element_count = N;
};

template <typename T, size_t N>
struct array_traits_t<redx::array<T, N>>
{
  using element_type = remove_cvref_t<T>;
  static constexpr size_t element_count = N;
};

} // namespace redx

