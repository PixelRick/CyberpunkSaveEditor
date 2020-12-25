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

  bool to_node(const std::shared_ptr<const node_t>& node)
  {
    // todo
    return true;
  }
};

struct inventory
{
  struct item_entry
  {
    namehash id;
    std::shared_ptr<const node_t> item_node;
  };

  std::vector<item_entry> m_items;
  uint32_t m_ukcnt0 = 0;
  uint64_t m_ukcnt1 = 0;
  uint32_t m_ukcnt2 = 0;

  bool from_node(const std::shared_ptr<const node_t>& node)
  {
    auto& data = node->data();
    std::vector<std::shared_ptr<const node_t>> children = node->children();
    if (!children.size() || children.size() % 2 != 1)
      return false;

    std::vector<char> first_blob = children[0]->nonconst().data();
    std::vector<char> last_blob = children.back()->nonconst().data();

    vector_streambuf<char> c0buf(children[0]->nonconst().data());
    std::istream is(&c0buf);
    is.read((char*)&m_ukcnt0, 4);
    is.read((char*)&m_ukcnt1, 8);
    is.read((char*)&m_ukcnt2, 4);

    if (is.eof() && children.size() == 1)
      return true;

    // prepare for iteration of items
    children[0] = node_t::create_shared_blob(first_blob, 16, (uint32_t)first_blob.size());
    children.pop_back();

    for (int i = 0; i < children.size(); i += 2)
    {
      m_items.emplace_back();
      item_entry& entry = m_items.back();

      auto& hdr = children[i];
      auto& itemdata = children[i+1];

      vector_streambuf<char> hdrbuf(hdr->nonconst().data());
      std::istream is(&hdrbuf);
      is >> entry.id;

      entry.item_node = itemdata;
    }
    return true;
  }

  bool to_node(const std::shared_ptr<const node_t>& node)
  {
    // todo
    return false;
  }
};

