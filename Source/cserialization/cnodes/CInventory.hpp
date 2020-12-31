#pragma once
#include <iostream>
#include <list>

#include "cserialization/node.hpp"
#include "cpinternals/cpnames.hpp"
#include "cserialization/packing.hpp"
#include "utils.hpp"
#include "CItemData.hpp"


struct sub_inventory_t
{
  uint64_t uid = 0;
  std::list<CItemData> items;
};


struct CInventory
{
  std::list<sub_inventory_t> m_subinvs;

  bool from_node(const std::shared_ptr<const node_t>& node)
  {
    if (!node)
      return false;

    node_reader reader(node);

    uint32_t inventory_cnt;
    reader.read((char*)&inventory_cnt, 4);
    m_subinvs.resize(inventory_cnt);

    for (auto& subinv : m_subinvs)
    {
      reader.read((char*)&subinv.uid, 8);

      uint32_t items_cnt;
      reader.read((char*)&items_cnt, 4);

      subinv.items.resize(items_cnt);
      for (auto& entry : subinv.items)
      {
          TweakDBID id;
        reader.read((char*)&id, 7);
        uint64_t uk;
        reader.read((char*)&uk, 8);

        auto item_node = reader.read_child("itemData");
        if (!item_node)
          return false; // todo: don't leave this in this state

        if (!entry.from_node(item_node))
          return false;
      }
    }

    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node()
  {
    node_writer writer;

    uint32_t inventory_cnt = (uint32_t)m_subinvs.size();
    writer.write((char*)&inventory_cnt, 4);

    for (auto& subinv : m_subinvs)
    {
      writer.write((char*)&subinv.uid, 8);

      uint32_t items_cnt = (uint32_t)subinv.items.size();
      writer.write((char*)&items_cnt, 4);

      for (auto& entry : subinv.items)
      {
        auto item_node = entry.to_node();
        if (!item_node)
          return nullptr; // todo: don't leave this in this state

        writer << entry.iid;
        writer.write_child(item_node);
      }
    }

    return writer.finalize("inventory");
  }
};

