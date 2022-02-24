#pragma once
#include <redx/core.hpp>
#include <redx/io/bstream.hpp>

namespace redx {

// serialization functions are not defined in serializables because it is on a per-game basis
// however types can be informed of serialization operations (e.g. to setup non reflected members)
struct serializable
{
  virtual ~serializable() = default;

  virtual bool serialize_in(ibstream& st) {};
  virtual bool serialize_out(ibstream& st) {};
};

} // namespace redxs

