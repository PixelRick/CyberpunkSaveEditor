#pragma once
#include "cpinternals/common.hpp"

// We don't need to exactly match the game rtti system since we only work on serialized data
// and do not execute scripts.

namespace cp::rtdt {

enum typekind
{
  native,       // includes vector<native>
  Simple,       // not iscriptable, simple collection of properties
  Class,        //     iscriptable
  Array,        // dynamic size array
  Enum,
  NativeArray,  // constant size array
  StaticArray,  // constant size array ?
  Handle,       // std::shared_ptr
  WeakHandle,   // std::weak_ptr
  ResourceReference,
  ResourceAsyncReference,
  BitField,
  LegacySingleChannelCurve,
  ScriptReference,
};

struct itype
{
  itype(gname name)
    : m_name(name)
    , m_cname(name)
  {
  }

  virtual ~itype() = default;

  gname name() const
  {
    return m_name;
  }

  CName cname() const
  {
    return m_cname;
  }

  virtual typekind kind() const = 0;

  virtual size_t size() const = 0;
  virtual size_t alignment() const = 0;

  virtual void construct(void* p) const = 0;
  virtual void destroy(void* p) const = 0;
  virtual void* allocate() const = 0;

  virtual bool serialize(iarchive& ar, void* p) const = 0;

  virtual bool hash(void* p) const = 0;

protected:
  gname m_name;
  CName m_cname;
};

} // namespace cp::rtdt

