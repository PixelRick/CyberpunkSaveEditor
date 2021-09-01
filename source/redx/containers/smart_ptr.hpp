#pragma once
#include <type_traits>
#include <utility>
#include <iterator>
#include <algorithm>
#include <memory>

#include <redx/core.hpp>
#include <redx/containers/type_allocation_traits.hpp>

namespace redx {

// wrap the std ones to ensure allocation policies are used ??
// no real need right ? deleters are smart

// new/delete are better than I thought in C++17:
// they do handle extended alignment without any change.

template <typename T>
using shared_ptr = std::shared_ptr<T>;

template <typename T>
using weak_ptr = std::weak_ptr<T>;

//template <typename T>
//struct shared_ptr
//  : std::shared_ptr<T>
//{
//protected:
//
//  template <typename U, class... Args>
//  friend shared_ptr<U> make_shared(Args&&... args);
//
//  using base = std::shared_ptr<T>;
//  using base::base;
//};
//
//template <typename T>
//struct weak_ptr
//  : std::weak_ptr<T>
//{
//  using base = std::weak_ptr<T>;
//  using base::base;
//};

namespace detail {

template <typename T>
struct custom_alloc_deleter_t
{
  void operator()(T* p) const
  {
    p->~T();
    stateless_allocator_t<T>::deallocate(static_cast<void*>(p), sizeof(T), 1, alignof(T));
  }
};

} // namespace detail

// does use custom allocator policy when set
template <typename T, class... Args>
[[nodiscard]] shared_ptr<T> make_shared_allocated(Args&&... args)
{
  using al_traits = typename detail::type_allocation_traits<T>;

  if constexpr (al_traits::uses_default_allocator)
  {
    // standard new/delete
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

  void* p = al_traits::allocator::allocate(sizeof(T), 1, alignof(T));
  ::new (p) T(std::forward<Args>(args)...);

  return shared_ptr<T>(static_cast<T*>(p), detail::custom_alloc_deleter_t<T>{});
}


//template <typename T>
//using shared_handle = std::shared_ptr<

} // namespace redx

