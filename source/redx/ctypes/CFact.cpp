#include "CFact.hpp"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace redx {

CFact::CFact(std::string_view name, uint32_t value, bool add_to_resolver)
  : m_value(value)
{
  m_hash = fnv1a32(name);
  if (add_to_resolver)
  {
    CFact_resolver::get().register_name(name);
  }
}

void CFact::name(gname name) 
{
  auto& resolver = CFact_resolver::get();
  m_hash = fnv1a32(name.strv());
  resolver.register_name(name);
}


void CFact_resolver::feed(const std::vector<gname>& names)
{
  m_list.reserve(m_list.size() + names.size());
  for (auto& name : names)
  {
    register_name(name);
  }
}

} // namespace redx

