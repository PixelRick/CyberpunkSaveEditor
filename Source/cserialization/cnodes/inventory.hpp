#pragma once
#include <iostream>

#include "cserialization/node.hpp"
#include "cserialization/cpnames.hpp"
#include "utils.hpp"

/*
------------------------
NOTES
-----------------------

struct_csav.qwordE0 + 648   is ver1

--------------------------------------
cnt = read(4)

vec<8> a(cnt)
vec<16> b(cnt)

for i in range(cnt)
  c = read(8)
  d = read(4)
  if d:
    break  <- thx IDA for this aweful control flow.. (--') goes to the "while 1"

label_loop <-  

---------
if !d:



---------

while 1
  read_string_by_hashid_if_v193p()  <- question is, can i get a json of names so that i compute the hashes myself ?
  e = read(4)
  if VER1 >= 97:
    f = read(1)
  if VER1 >= 190:
    g = read(2)

*/

struct itemData
{
  std::shared_ptr<const node_t> raw;

  /*
  Seberoth
  itemData 0x08 - 0x0B seems to be another hash and if it is equal 2 then the item is stackable. 0x14 - 0x17 is the quantity (int32) (or 0 if its not stackable)
  */


  // (8, 4, 1, 2), 1, 4, 8, 4, 4, 8, 4, 1, 2, 1, 4, 8, 1, 8, 4, 1, 2, 1, 4, 8, 1, 4, 8, 4, 4, 8, 4, 1, 2, 1, 4, 8, 1, 4, 8, 4, 4, 8, 4, 1, 2, 1, 4, 8, 1, 4, 8, 4, 4, 4, 8, 4, 4, 
  // (8, 4, 1, 2), 1, 4, 8, 4, 4, 8, 4, 1, 2, 1, 7, 8, 1, 4, 8, 4, 4, 

  bool from_node(const std::shared_ptr<const node_t>& node)
  {
    if (!node)
      return false;
    raw = node;

    node_reader reader(node);

    namehash id;
    reader.read((char*)&id.as_u64, 8);

    uint32_t uk4;
    reader.read((char*)&uk4, 4);
    
    uint8_t uk1;
    reader.read((char*)&uk1, 1);

    uint16_t uk2;
    reader.read((char*)&uk2, 2);


    // todo
    return true;
  }

  std::shared_ptr<const node_t> to_node()
  {
    // todo
    return raw;
  }

  std::string name()
  {
    auto& buf = raw->data();
    if (buf.size() < 7)
      return "invalid: no namehash in item data";
    namehash id = *(namehash*)buf.data();
    id._pad = 0;
    if (id.uk[0] || id.uk[1])
      return "invalid: namehash is malformed";
    auto& cpn = cpnames::get();
    return cpn.get_name(id);
  }
};


struct subinv_t
{
  uint64_t uid = 0;
  std::vector<itemData> items;
};


struct inventory
{
  std::vector<subinv_t> m_subinvs;

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
        namehash id;
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

        // for now it is hacky, i use the first 15 bytes of itemData..
        auto& item_buf = item_node->data();
        writer.write(item_buf.data(), 15);
      
        writer.write_child(item_node);
      }
    }

    return writer.finalize("inventory");
  }
};

