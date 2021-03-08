#pragma once
#include "itype.hpp"
#include "idynamic.hpp"

namespace cp::rtdt {

struct Simple_type
  : idynamic_type
{
  Simple_type(gname name)
    : idynamic_type(name)
  {
  }

  ~Simple_type() override = default;

  typekind kind() const override
  {
    return typekind::Simple;
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

  bool serialize(streambase& ar, void* p) const override
  {
  }

protected:
  size_t m_size;
  size_t m_alignment;
};

} // namespace cp::rtdt

