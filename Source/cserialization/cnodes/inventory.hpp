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

  bool from_node(const std::shared_ptr<const node_t>& node)
  {
    raw = node;
    // todo
    return true;
  }

  std::shared_ptr<const node_t> to_node()
  {
    // todo
    return raw;
  }
};

struct inventory
{
  struct item_entry_t
  {
    namehash id;
    itemData item;
  };

  struct subinv_t
  {
    uint32_t ukcnt0 = 0;
    uint64_t ukcnt1 = 0;
    uint32_t ukcnt2 = 0;
    std::vector<item_entry_t> items;
  };

  std::vector<subinv_t> m_subinvs;
  
  bool from_node(const std::shared_ptr<const node_t>& node)
  {
    if (!node)
      return false;

    node_reader reader(node);

    while (!reader.at_end())
    {
      m_subinvs.emplace_back();
      subinv_t& subinv = m_subinvs.back();
      reader.read((char*)&subinv.ukcnt0, 4);
      reader.read((char*)&subinv.ukcnt1, 8);
      reader.read((char*)&subinv.ukcnt2, 4);

      for (uint32_t i = 0; i < subinv.ukcnt2; ++i)
      {
        subinv.items.emplace_back();
        item_entry_t& entry = subinv.items.back();

        reader.read((char*)&entry.id.as_u64, 7);

        auto item_node = reader.read_child("itemData");
        if (!item_node)
          return false;

        entry.item.from_node(item_node);
      }
    }

    return true;
  }

  bool to_node(const std::shared_ptr<const node_t>& node)
  {
    if (!node)
      return false;

    node_writer writer;
    return false;

    //std::vector<std::shared_ptr<const node_t>> child_nodes;
    //
    //for (auto& e : m_items)
    //{
    //  std::stringstream ss;
    //  ss << e.id;
    //  auto blob = ss.str();
    //
    //  // todo, use id from itemData when it is reversed
    //
    //  child_nodes.emplace_back(
    //    node_t::create_shared_blob(blob.data(), 0, (uint32_t)blob.size())
    //  );
    //  //child_nodes.emplace_back(e.item_node);
    //}


    // to finish

    return false;
  }
};

