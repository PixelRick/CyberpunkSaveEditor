#pragma once
#include "itype.hpp"
#include <vector>

namespace cp::rtdt {

struct attribute
{
  attribute() = default;

  attribute(gname name, itype* type)
    : m_name(name), m_type(type)
  {
  }

  gname name() const
  {
    return m_name;
  }

  itype* type() const
  {
    return m_type;
  }

protected:
  gname m_name;
  itype* m_type = nullptr;
};


struct dynamic_type
  : itype
{
  using itype::itype;

  const std::vector<attribute>& attr()
  {
    return m_attrs;
  }

  itype* base_type()
  {
    return m_base;
  }

protected:
  itype* m_base = nullptr;
  std::vector<attribute> m_attrs;
};


struct iscriptable
  : iserializable
{
  ~iscriptable() override = default;

  virtual itype* native_type() const = 0;
};

} // namespace cp::rtdt

