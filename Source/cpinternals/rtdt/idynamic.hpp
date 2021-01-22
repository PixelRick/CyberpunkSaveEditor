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


struct idynamic_type
  : itype
{
  using itype::itype;

protected:
  // friend..

  const std::vector<attribute>& attr()
  {
    return m_attrs;
  }

  std::vector<attribute> m_attrs;
};

// All considered dynamic types are serializable.
struct idynamic
  : iserializable
{
  ~idynamic() override = default;

  virtual itype* native_type() const = 0;
};

} // namespace cp::rtdt

