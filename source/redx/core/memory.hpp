#pragma once
#include <new>
#include <algorithm>

namespace redx {

struct aligned_storage_allocator
{
  [[nodiscard]] static inline void* allocate(size_t size, size_t alignment)
  {
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
    {
      // microsoft trick to help autovectorization
      size_t al_alignment = (size > 0x1000) ? std::max(alignment, 32ull): alignment;
      return ::operator new (size, std::align_val_t{al_alignment});
    }
    else
    {
      return ::operator new (size);
    }
  }

  static inline void deallocate(void* p, size_t size, size_t alignment)
  {
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
    {
      // microsoft trick to help autovectorization
      size_t al_alignment = (size > 0x1000) ? std::max(alignment, 32ull): alignment;
      ::operator delete (p, size, std::align_val_t{al_alignment});
    }
    else
    {
      ::operator delete (p, size);
    }
  }
};

} // namespace redx

