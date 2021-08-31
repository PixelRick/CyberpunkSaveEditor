#include "TweakDBID.hpp"

#include "redx/common.hpp"

namespace redx {

TweakDBID::TweakDBID(std::string_view name, bool add_to_resolver)
  : as_u64(0)
{
  const size_t ssize = name.size();
  if (ssize > 0xFF)
    throw std::length_error("TweakDBID's length overflow");

  crc = crc32_str(name);
  slen = static_cast<uint8_t>(ssize);
  if (add_to_resolver)
  {
    TweakDBID_resolver::get().register_name(name);
  }
}

void TweakDBID_resolver::register_name(gname name)
{
  auto sv = name.strv();

  auto p = insert_sorted_nodupe(m_full_list, name);
  if (!p.second)
    return;

  if (sv.rfind("Ammo.", 0) == 0)
  {
    insert_sorted(m_item_list, name);
  }
  else if (sv.rfind("Items.", 0) == 0)
  {
    insert_sorted(m_item_list, name);
    std::string s(sv);
    insert_sorted_nodupe(m_full_list, gname(s + "_Rare"));
    insert_sorted_nodupe(m_full_list, gname(s + "_Epic"));
    insert_sorted_nodupe(m_full_list, gname(s + "_Legendary"));
  }
  else if (sv.rfind("AttachmentSlots.", 0) == 0)
  {
    insert_sorted(m_attachment_list, name);
    std::string s(sv); 
    insert_sorted_nodupe(m_full_list, gname(s + "_Rare"));
    insert_sorted_nodupe(m_full_list, gname(s + "_Epic"));
    insert_sorted_nodupe(m_full_list, gname(s + "_Legendary"));
  }
  else if (sv.rfind("Vehicle.v_", 0) == 0)
  {
    insert_sorted(m_vehicle_list, name);
  }
  else if (sv.rfind("Vehicle.av_", 0) == 0)
  {
    insert_sorted(m_vehicle_list, name);
  }
  else
  {
    insert_sorted(m_unknown_list, name);
  }

  TweakDBID id(name, false);
  m_tdbid_invmap[id.as_u64] = name;
  m_crc32_invmap[id.crc] = name;
}

void TweakDBID_resolver::feed(const std::vector<gname>& names)
{
  m_full_list.reserve(m_full_list.size() + names.size());
  for (auto& name : names)
  {
    register_name(name);
  }
}

} // namespace redx

