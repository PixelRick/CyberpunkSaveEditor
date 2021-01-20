#include "CName.hpp"

#include "cpinternals/common.hpp"

namespace cp {

CName::CName(std::string_view name, bool add_to_resolver)
{
  as_u64 = fnv1a64(name);
  if (add_to_resolver)
  {
    CName_resolver::get().register_name(name);
  }
}


void CName_resolver::register_name(gname name)
{
  auto sv = name.strv();

  auto it = insert_sorted_nodupe(m_full_list, name);
  if (it == m_full_list.end())
    return;

  CName id(name, false);
  m_invmap[id.as_u64] = name;
  m_invmap_32[fnv1a32(sv)] = name;
}

void CName_resolver::feed(const std::vector<gname>& names)
{
  m_full_list.reserve(m_full_list.size() + names.size());
  for (auto& name : names)
  {
    register_name(name);
  }
}

} // namespace cp

