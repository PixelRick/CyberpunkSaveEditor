#pragma once
#include "itype.hpp"

namespace cp::rtdt {

// todo later if necessary: make it inative_type and specialize by kind to not have to store m_kind..

template <typename T>
struct native_type
  : itype
{
  native_type(gname name, typekind kind)
    : itype(name)
    , m_kind(kind)
  {
  }

  ~native_type() override = default;

  typekind kind() const override
  {
    return m_kind;
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

protected:
  typekind m_kind;
};

} // namespace cp::rtdt

