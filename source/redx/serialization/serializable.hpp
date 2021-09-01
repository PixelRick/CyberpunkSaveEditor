#pragma once
#include <redx/core.hpp>
#include <redx/io/bstream.hpp>

namespace redx {

// so that scriptables can serialize more than their reflected properties..
struct serializable
{
  virtual ~serializable() = default;

  virtual bool serialize_in(ibstream& st) {};
  virtual bool serialize_out(ibstream& st) {};
};

} // namespace redxs

