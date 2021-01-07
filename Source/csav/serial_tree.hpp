#pragma once
#include <iostream>
#include <memory>
#include <csav/node.hpp>
#include <csav/serializers.hpp>


struct serial_node_desc
{
  std::string name;
  int32_t next_idx, child_idx;
  uint32_t data_offset, data_size;

  friend std::istream& operator>>(std::istream& is, serial_node_desc& ed)
  {
    is >> cp_plstring_ref(ed.name);
    is >> cbytes_ref(ed.next_idx   ) >> cbytes_ref(ed.child_idx);
    is >> cbytes_ref(ed.data_offset) >> cbytes_ref(ed.data_size);
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const serial_node_desc& ed)
  {
    os << cp_plstring_ref(ed.name);
    os << cbytes_ref(ed.next_idx   ) << cbytes_ref(ed.child_idx);
    os << cbytes_ref(ed.data_offset) << cbytes_ref(ed.data_size);
    return os;
  }
};


class serial_tree
{
public:
  std::vector<serial_node_desc> descs;
  std::vector<char> nodedata;

public:
  serial_tree() = default;

  bool from_node(const std::shared_ptr<const node_t>& node, uint32_t data_offset)
  {
    // yes that looks dumb, but cdpred use first data_offset = min_offset
    // so before creating the node descriptors i fill the buffer to min_offset
    nodedata.resize(data_offset);

    uint32_t node_cnt = node->treecount();

    descs.resize(node_cnt);
    uint32_t next_idx = 0;
    write_node_children(*node, next_idx);

    // check that each blob starts with its node index (dword)
    size_t i = 0;
    for (auto& ed : descs)
    {
      if (*(uint32_t*)(nodedata.data() + ed.data_offset) != i++)
        return false;
    }

    return true;
  }

  std::shared_ptr<const node_t> to_node(uint32_t data_offset)
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
    serial_node_desc root_desc {"root", node_t::null_node_idx, 0, data_offset, data_size};
    return read_node(root_desc, node_t::root_node_idx);
  }

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
