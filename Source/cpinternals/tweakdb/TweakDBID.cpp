#include "TweakDBID.hpp"

namespace cp {

void TweakDBID_resolver::feed(const std::vector<gname>& names)
{
  m_full_list.reserve(m_full_list.size() + names.size());

  for (auto& name : names)
  {
    auto sv = name.strv();

    insert_sorted(m_full_list, name);

    if (sv.rfind("Ammo.", 0) == 0)
    {
      insert_sorted(m_item_list, name);
    }
    else if (sv.rfind("Items.", 0) == 0)
    {
      insert_sorted(m_item_list, name);
      std::string s(sv);
      insert_sorted(m_full_list, gname(s + "_Rare"));
      insert_sorted(m_full_list, gname(s + "_Epic"));
      insert_sorted(m_full_list, gname(s + "_Legendary"));
    }
    else if (sv.rfind("AttachmentSlots.", 0) == 0)
    {
      insert_sorted(m_attachment_list, name);
      std::string s(sv); 
      insert_sorted(m_full_list, gname(s + "_Rare"));
      insert_sorted(m_full_list, gname(s + "_Epic"));
      insert_sorted(m_full_list, gname(s + "_Legendary"));
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

    TweakDBID id(crc32(sv), sv.size());
    m_tdbid_invmap[id.as_u64] = name;
    m_crc32_invmap[id.crc] = name;
  }
}

} // namespace cp

