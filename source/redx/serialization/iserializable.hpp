#pragma once
#include <inttypes.h>
#include "misc.hpp"

namespace redx {

struct streambase;

struct iserializable
{
  virtual ~iserializable() = default;

  virtual bool serialize(streambase& ar) = 0;
};

} // namespace redx

