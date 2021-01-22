#pragma once
#include "itype.hpp"

namespace cp::rtdt {

struct dyn_vector_type
  : itype
{
  dyn_vector_type(gname name)
    : itype(name)
  {
  }

  ~dyn_vector_type() override = default;

  typekind kind() const override
  {
    return typekind::dyn_vector;
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

};

} // namespace cp::rtdt

