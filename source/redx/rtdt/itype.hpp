#pragma once
#include "cpinternals/common.hpp"

// We don't need to exactly match the game rtti system since we only work on serialized data
// and do not execute scripts.

namespace cp::rtdt {

enum typekind
{
  fundamental,  //
  Simple,       // not iscriptable, simple collection of properties in native
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

  virtual bool serialize(streambase& ar, void* p) const = 0;

  virtual bool hash(void* p) const = 0;

protected:
  gname m_name;
  CName m_cname;
};

struct types_mgr
{
  static types_mgr& instance()
  {
    static types_mgr s;
    return s;
  }

  types_mgr(const types_mgr&) = delete;
  types_mgr& operator=(const types_mgr&) = delete;

private:
  types_mgr() {}
  ~types_mgr() {}

  std::unordered_map<gname, std::unique_ptr<itype>> m_types;
};


} // namespace cp::rtdt

