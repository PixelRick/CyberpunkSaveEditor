#pragma once
#include <inttypes.h>

namespace cp {

struct iarchive;

struct iserializable
{
  virtual ~iserializable() = default;

  virtual bool serialize(iarchive& ar) = 0;
};

} // namespace cp

