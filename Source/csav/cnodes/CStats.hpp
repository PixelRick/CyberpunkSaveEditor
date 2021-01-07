#pragma once
#include <inttypes.h>

#include <cpinternals/cpnames.hpp>
#include <csav/node.hpp>
#include <csav/serializers.hpp>
#include <csav/csystem/CSystem.hpp>
#include <csav/csystem/CProperty.hpp>

struct CStats
  : public node_serializable
{
  std::shared_ptr<const node_t> m_raw; // temporary
  CSystem m_sys;

  CObjectSPtr m_mapstruct;
  std::shared_ptr<CDynArrayProperty> m_stats_values_object;

  // precompute this
  // todo: listen to system objects and update it when necessary
  std::unordered_map<uint32_t, std::shared_ptr<CDynArrayProperty>> m_seed_to_modifiers_map;


public:
  CStats() = default;

  const CSystem& system() const { return m_sys; }
  CSystem& system() { return m_sys; }

public:

  // tools
  CPropertySPtr get_modifiers_prop(uint32_t seed)
  {
    auto it = m_seed_to_modifiers_map.find(seed);
    if (it == m_seed_to_modifiers_map.end())
      return nullptr;
    return std::static_pointer_cast<CProperty>(it->second);
  }

protected:
  void add_new_modifier(const CPropertySPtr& modifiers, CSysName name)
  {
    auto mods = std::dynamic_pointer_cast<CDynArrayProperty>(modifiers);
    if (!mods)
      return;
    auto new_handle = std::dynamic_pointer_cast<CHandleProperty>(*mods->emplace(mods->begin()));
    if (!new_handle)
      return;//damnit
    new_handle->set_obj(std::make_shared<CObject>(name));
  }

public:
  void add_combined_stats(const CPropertySPtr& modifiers)
  {
    add_new_modifier(modifiers, "gameCombinedStatModifierData");
  }

  void add_curve_stats(const CPropertySPtr& modifiers)
  {
    add_new_modifier(modifiers, "gameCurveStatModifierData");
  }

  void add_constant_stats(const CPropertySPtr& modifiers)
  {
    add_new_modifier(modifiers, "gameConstantStatModifierData");
  }

public:
  std::string node_name() const override { return "StatsSystem"; }

  bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) override
  {
    if (!node)
      return false;

    m_raw = node;

    node_reader reader(node, version);

    // todo: catch exception at upper level to display error
    if (!m_sys.serialize_in(reader))
      return false;

    if (!reader.at_end())
      return false;

    // this isn't editor breaking, so we return true if it fails

    m_mapstruct = m_sys.objects()[0];
    if (!m_mapstruct)
      return true;

    auto mapvalues = m_mapstruct->get_prop("values");
    m_stats_values_object = std::dynamic_pointer_cast<CDynArrayProperty>(mapvalues);
    if (!m_stats_values_object)
      return true;

    // iterate over gameSavedStatsData objects
    for (auto& it : *m_stats_values_object)
    {
      std::shared_ptr<CObjectProperty> objprop = std::dynamic_pointer_cast<CObjectProperty>(it);
      if (!objprop)
        continue;
      auto obj = objprop->obj();
      auto seedprop = std::dynamic_pointer_cast<CIntProperty>(obj->get_prop("seed"));
      if (!seedprop)
        continue;
      uint32_t seed = seedprop->u32();
      auto modsarray = std::dynamic_pointer_cast<CDynArrayProperty>(obj->get_prop("statModifiers"));
      if (!modsarray)
        continue;
      m_seed_to_modifiers_map.emplace(seed, modsarray);
    }

    return true;
  }

  std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const override
  {
    node_writer writer(version);

    try
    {
      if (!m_sys.serialize_out(writer))
        return nullptr;
    }
    catch (std::exception&)
    {
      return nullptr;
    }

    return writer.finalize(node_name());
  }
};


