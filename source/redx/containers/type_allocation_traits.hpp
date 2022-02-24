#pragma once
#include <type_traits>
#include <utility>
#include <iterator>
#include <algorithm>
#include <initializer_list>

#include <redx/core.hpp>

// you can define an allocation policy for structs by specifying
// a stateless allocator, this way:
// 
// struct some_type {
//   using stateless_allocator = some_allocator<some_type>;
// };
//

namespace redx {

// rttr requires a polymorphic allocator class.
// Having a different one for each T is redundant when the standard allocator
// actually just calls new/delete with a few optimizations based on aligment and allocation total size.
// So the idea here is to have a default allocator that isn't type-aware, but only aware of alignment.
// This will help reduce the number of allocator instances.
// This implies the allocator::allocate won't return T*.. thus let's just specify
// that custom allocators's allocate should return void*..
// type-aware allocators can still be used for specific types, but should provide the same
// interface as below and check that they are used with the correct elt_size and alignment!

template <size_t Alignment>
struct default_stateless_allocator
{
private:

  using compatible_storage = std::aligned_storage<1, Alignment>;
  using underlying_allocator = std::allocator<std::aligned_storage<1, Alignment>>;

public:

  static inline void* allocate(size_t count, size_t elt_size, size_t alignment)
  {
    REDX_ASSERT(elt_size % Alignment == 0);
    REDX_ASSERT(Alignment == alignment);
    return static_cast<void*>(underlying_allocator().allocate(elt_size * count));
  }

  template <typename T>
  static inline void* allocate(size_t count)
  {
    return allocate(count, sizeof(T), alignof(T));
  }

  static inline void deallocate(void* p, size_t count, size_t elt_size, size_t alignment)
  {
    underlying_allocator().deallocate(static_cast<compatible_storage*>(p), elt_size * count);
  }

  template <typename T>
  static inline void deallocate(void* p, size_t count)
  {
    deallocate(p, count, sizeof(T), alignof(T));
  }
};

namespace detail {

template <typename T, class = void>
struct type_allocation_traits
{
  using allocator = redx::default_stateless_allocator<alignof(T)>;
  static constexpr bool uses_default_allocator = true;
};

template <typename T>
struct type_allocation_traits<T, std::void_t<typename T::stateless_allocator>>
{
  using allocator = typename T::stateless_allocator;
  static constexpr bool uses_default_allocator = false;
  static_assert(std::is_empty_v<allocator>, "stateless_allocator must be stateless..");
};

} // namespace detail

template <typename T>
using stateless_allocator_t = typename detail::type_allocation_traits<T>::allocator;

// same alignment should refer to same allocator (when no custom allocator is specified)
static_assert(std::is_same_v<stateless_allocator_t<int32_t>, stateless_allocator_t<uint32_t>>);



//template <typename T>
//struct aligned_newdelete_overload
//{
//  static void* operator new(std::size_t sz)
//  {
//  }
//
//};



} // namespace redx

