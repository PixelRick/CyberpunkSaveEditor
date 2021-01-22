#pragma once
#include <inttypes.h>
#include "misc.hpp"

namespace cp {

struct iarchive;

struct iserializable
{
  virtual ~iserializable() = default;

  virtual bool serialize(iarchive& ar) = 0;
};

} // namespace cp

