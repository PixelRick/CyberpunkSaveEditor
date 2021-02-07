#pragma once
#include "itype.hpp"

namespace cp::rtdt {

template <typename T, const char* Name>
struct native_type
  : itype
{
  native_type()
    : itype(gname(Name))
  {
  }

  ~native_type() override = default;

  typekind kind() const override
  {
    return typekind::native;
  }

  size_t size() const override
  {
    return sizeof(T);
  }

  size_t alignment() const override
  {
    return alignof(T);
  }

  void construct(void* p) const override
  {
    ::new (p) T();
  }

  void destroy(void* p) const override
  {
    static_cast<T*>(p)->~T();
  }

  void* allocate() const override
  {
    return new T();
  }

  bool serialize(iarchive& ar, void* p) const override
  {
    ar << *static_cast<T*>(p);
  }
};

} // namespace cp::rtdt

