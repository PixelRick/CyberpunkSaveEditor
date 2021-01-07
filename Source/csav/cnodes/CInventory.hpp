#pragma once
#include <iostream>
#include <list>

#include <utils.hpp>
#include <cpinternals/cpnames.hpp>
#include <csav/node.hpp>
#include <csav/serializers.hpp>
#include "CItemData.hpp"


struct sub_inventory_t
{
  uint64_t uid = 0;
  std::list<CItemData> items;
};

struct CInventory
  : public node_serializable
{
  std::list<sub_inventory_t> m_subinvs;

  std::string node_name() const override { return "inventory"; }

  bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) override
  {
    if (!node)
      return false;

    node_reader reader(node, version);

    try
    {
      uint32_t inventory_cnt = 0;
      reader >> cbytes_ref(inventory_cnt);
      m_subinvs.resize(inventory_cnt);

      for (auto& subinv : m_subinvs)
      {
        reader >> cbytes_ref(subinv.uid);

        uint32_t items_cnt;
        reader >> cbytes_ref(items_cnt);

        subinv.items.resize(items_cnt);
        for (auto& entry : subinv.items)
        {
          TweakDBID id;
          reader.read((char*)&id, 7);

          uint64_t uk;
          reader>> cbytes_ref(uk);

          auto item_node = reader.read_child("itemData");
          if (!item_node)
            return false; // todo: don't leave this in this state

          if (!entry.from_node(item_node, version))
            return false;
        }
      }
    }
    catch (std::ios::failure&)
    {
      return false;
    }


    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const override
  {
    node_writer writer(version);

    uint32_t inventory_cnt = (uint32_t)m_subinvs.size();
    writer.write((char*)&inventory_cnt, 4);

    for (auto& subinv : m_subinvs)
    {
      writer.write((char*)&subinv.uid, 8);

      uint32_t items_cnt = (uint32_t)subinv.items.size();
      writer.write((char*)&items_cnt, 4);

      for (const auto& entry : subinv.items)
      {
        auto item_node = entry.to_node(version);
        if (!item_node)
          return nullptr; // todo: don't leave this in this state

        writer << entry.iid;
        writer.write_child(item_node);
      }
    }

    return writer.finalize(node_name());
  }
};

