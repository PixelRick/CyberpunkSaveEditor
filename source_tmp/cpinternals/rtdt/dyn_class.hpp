#pragma once
#include "itype.hpp"
#include <vector>
#include "idynamic.hpp"

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


struct dyn_class_instance
  : idynamic
{
  ~dyn_class_instance() override = default;

  virtual itype* native_type() const = 0;


};


struct dyn_class_type
  : itype
{
  dyn_class_type(gname name)
    : itype(name)
  {
  }

  ~dyn_class_type() override = default;

  typekind kind() const override
  {
    return typekind::dyn_class;
  }

  size_t size() const override
  {
    return m_size;
  }

  size_t alignment() const override
  {
    return m_alignment;
  }

  void construct(void* p) const override
  {
  }

  void destroy(void* p) const override
  {
  }

  void* allocate() const override
  {
  }

  bool serialize(iarchive& ar, void* p) const override
  {
  }



protected:
  size_t m_size;
  size_t m_alignment;

  std::vector<attribute> m_attrs;
};

} // namespace cp::rtdt

