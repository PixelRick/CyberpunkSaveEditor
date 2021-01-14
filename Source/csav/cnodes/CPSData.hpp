#pragma once
#include <inttypes.h>

#include <cpinternals/cpnames.hpp>
#include <csav/node.hpp>
#include <csav/serializers.hpp>
#include <csav/csystem/CSystem.hpp>


struct CPSData
  : public node_serializable
{
  std::shared_ptr<const node_t> m_raw; // temporary
  CSystem m_sys;
  std::vector<CName> trailing_names;
  std::set<std::string> m_obj_ctypenames;

  CObjectSPtr m_vehicleGarageComponentPS;

  std::vector<std::string> m_vehicle_names;

public:
  CPSData() = default;


  const CSystem& system() const { return m_sys; }
        CSystem& system()       { return m_sys; }


  // some fun
  bool replace_spawned_vehicles(TweakDBID tdbid)
  {
      if (!m_vehicleGarageComponentPS)
        return false;

      auto pdata = m_vehicleGarageComponentPS->get_prop_cast<CDynArrayProperty>("spawnedVehiclesData");
      if (!pdata)
        return false;
      for (auto& item : *pdata)
      {
        auto objprop = dynamic_cast<CObjectProperty*>(item.get());
        if (!objprop)
          return false;
        auto obj = objprop->obj();
        if (!obj)
          return false;
        auto record = obj->get_prop_cast<CTweakDBIDProperty>("spawnRecordID");
        if (!record)
          return false;
        record->m_id = tdbid;
      }

      return true;
  }


  std::string node_name() const override { return "PSData"; }


  bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) override
  {
    if (!node)
      return false;

    m_raw = node;

    node_reader reader(node, version);

    // todo: catch exception at upper level to display error
    if (!m_sys.serialize_in(reader))
      return false;

    uint32_t cnt = 0;
    reader >> cbytes_ref(cnt);
    trailing_names.resize(cnt);
    reader.read((char*)trailing_names.data(), cnt * sizeof(CName));

    CSysName vgcps("vehicleGarageComponentPS");

    for (auto& obj : m_sys.objects())
    {
      auto cn = obj->ctypename();
      m_obj_ctypenames.insert(cn.str());
      if (cn == vgcps)
        m_vehicleGarageComponentPS = obj;
    }

    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const override
  {
    node_writer writer(version);

    try
    {
      if (!m_sys.serialize_out(writer))
        return nullptr;

      uint32_t cnt = (uint32_t)trailing_names.size();
      writer << cbytes_ref(cnt);
      writer.write((char*)trailing_names.data(), cnt * sizeof(CName));
    }
    catch (std::exception&)
    {
      return nullptr;
    }

    return writer.finalize(node_name());
  }
};


