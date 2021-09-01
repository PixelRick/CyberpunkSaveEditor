#pragma once
#include <iostream>
#include <memory>

#include "redx/core.hpp"
#include "redx/io/bstream.hpp"
#include "redx/ctypes.hpp"
#include "redx/csav/node.hpp"

#include "redx/csav/serializers.hpp"

namespace redx::csav {

struct serial_node_desc
{
  static constexpr bool is_serializable_pod = false;

  int32_t next_idx, child_idx;
  uint32_t data_offset, data_size;
  std::string name;
};

inline ibstream& operator>>(ibstream& st, serial_node_desc& x)
{
  st.read_str_lpfxd(x.name);
  st.read_bytes((char*)&x, offsetof(serial_node_desc, name));
  return st;
}

inline obstream& operator<<(obstream& st, const serial_node_desc& x)
{
  st.write_str_lpfxd(x.name);
  st.write_bytes((const char*)&x, offsetof(serial_node_desc, name));
  return st;
}

struct serial_tree
{
  serial_tree() = default;

  bool from_tree(const std::shared_ptr<const node_t>& root, uint32_t data_offset)
  {
    // yes that looks dumb, but cdpred use first data_offset = min_offset
    // so before creating the node descriptors i fill the buffer to min_offset
    nodedata.resize(data_offset);

    uint32_t node_cnt = root->treecount();

    descs.resize(node_cnt);
    uint32_t next_idx = 0;
    write_node_children(*root, next_idx);

    // check that each blob starts with its node index (dword)
    size_t i = 0;
    for (auto& ed : descs)
    {
      if (*(uint32_t*)(nodedata.data() + ed.data_offset) != i++)
        return false;
    }

    return true;
  }

  std::shared_ptr<const node_t> to_tree(uint32_t data_offset)
  {
    // check that each blob starts with its node index (dword)
    size_t i = 0;
    for (auto& nd : descs)
    {
      if (*(uint32_t*)(nodedata.data() + nd.data_offset) != i++)
        return nullptr;
    }

    // fake descriptor, our buffer should be prefixed with zeroes so the *data==idx will pass..
    const uint32_t data_size = (uint32_t)nodedata.size() - data_offset;
    serial_node_desc root_desc {node_t::null_node_idx, 0, data_offset, data_size, "root"};
    return read_node(root_desc, node_t::root_node_idx);
  }

  std::vector<serial_node_desc> descs;
  std::vector<char> nodedata;

protected:
  std::shared_ptr<const node_t> read_node(serial_node_desc& desc, int32_t idx)
  {
    uint32_t cur_offset = desc.data_offset + 4;
    uint32_t end_offset = desc.data_offset + desc.data_size;

    // root node has no u32 idx
    if (idx == node_t::root_node_idx)
      cur_offset = desc.data_offset;

    if (end_offset > nodedata.size())
      return nullptr;

    if (*(uint32_t*)(nodedata.data() + desc.data_offset) != idx && idx != node_t::root_node_idx)
      return nullptr;

    auto node = node_t::create_shared(idx, desc.name);
    auto& nc_node = node->nonconst();

    std::vector<std::shared_ptr<const node_t>> children;

    if (desc.child_idx >= 0)
    {
      int i = desc.child_idx;
      while (i >= 0)
      {
        if (i >= descs.size()) // corruption ?
          return nullptr;

        auto& childdesc = descs[i];

        if (childdesc.data_offset > cur_offset) {
          children.push_back(
            node_t::create_shared_blob(nodedata.data(), cur_offset, childdesc.data_offset)
          );
        }

        auto childnode = read_node(childdesc, i);
        if (!childnode) // something went wrong
          return nullptr;
        children.push_back(childnode);

        cur_offset = childdesc.data_offset + childdesc.data_size;
        i = childdesc.next_idx;
      }

      if (cur_offset < end_offset) {
        children.push_back(
          node_t::create_shared_blob(nodedata.data(), cur_offset, end_offset)
        );
      }

      nc_node.assign_children(children.begin(), children.end());
    }
    else if (cur_offset < end_offset)
    {
      nc_node.assign_data(
        nodedata.begin() + cur_offset,
        nodedata.begin() + end_offset
      );
    }

    return node;
  }

  serial_node_desc* write_node_visitor(const node_t& node, uint32_t& next_idx)
  {
    if (node.idx() >= 0)
    {
      const uint32_t idx = next_idx++;
      node.nonconst().idx(idx);

      auto& nd = descs[idx];
      nd.name = node.name();
      nd.data_offset = (uint32_t)nodedata.size();
      nd.child_idx = node.has_children() ? next_idx : node_t::null_node_idx;

      char* pIdx = (char*)&idx;
      std::copy(pIdx, pIdx + 4, std::back_inserter(nodedata));
      std::copy(node.data().begin(), node.data().end(), std::back_inserter(nodedata));

      write_node_children(node, next_idx);

      nd.next_idx = (next_idx < descs.size()) ? next_idx : node_t::null_node_idx;
      nd.data_size = (uint32_t)nodedata.size() - nd.data_offset;
      return &nd;
    }
    else
    {
      // data blob
      std::copy(node.data().begin(), node.data().end(), std::back_inserter(nodedata));
    }
    return nullptr;
  }

  void write_node_children(const node_t& node, uint32_t& next_idx)
  {
    serial_node_desc* last_child_desc = nullptr;
    for (auto& c : node.children())
    {
      auto cnd = write_node_visitor(*c, next_idx);
      if (cnd != nullptr)
        last_child_desc = cnd;
    }
    if (last_child_desc)
      last_child_desc->next_idx = node_t::null_node_idx;
  }
};

} // namespace redx::csav

