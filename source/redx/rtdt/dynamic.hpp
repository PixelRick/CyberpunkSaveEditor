#pragma once
#include "itype.hpp"

#include <vector>

namespace redx::rtdt {

struct iscriptable
  : iserializable
{
  //RTTR_ENABLE();

public:
  ~iscriptable() override = default;
  iscriptable()
  {
    //prop_default_states.resize(get_type().get_properties().size(), true);
  }

protected:
  std::vector<bool> prop_default_states;

};

} // namespace redx::rtdt

