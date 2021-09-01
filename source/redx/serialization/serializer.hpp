#pragma once
#include <inttypes.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <optional>

#include <redx/core/platform.hpp>
#include <redx/core/utils.hpp>
#include <redx/io/bstream.hpp>
#include <redx/serialization/serializable.hpp>

namespace redx {

// base input serializer for cp types, extend it to serialize cp types differently !
// it inherits ibstream :)
struct base_iserializer
  : ibstream
{
  using ibstream::ibstream;

  ~base_iserializer() override = default;



};


} // namespace redx

