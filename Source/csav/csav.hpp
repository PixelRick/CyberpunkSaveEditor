#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <numeric>
#include <cassert>
#include "xlz4/lz4.h"
#include "serializers.hpp"
#include "node.hpp"
#include "csav_version.hpp"

#include <csav/cnodes.hpp>
#include <csav/serial_tree.hpp>

#define XLZ4_CHUNK_SIZE 0x40000

struct compressed_chunk_desc
{
  static const size_t serialized_size = 12;

  // data_size is uncompressed size
  uint32_t offset, size, data_size, data_offset;

  friend std::istream& operator>>(std::istream& is, compressed_chunk_desc& cd)
  {
    is >> cbytes_ref(cd.offset) >> cbytes_ref(cd.size) >> cbytes_ref(cd.data_size);
    cd.data_offset = 0;
    return is;
  }

  friend std::ostream& operator<<(std::ostream& os, const compressed_chunk_desc& cd)
  {
    os << cbytes_ref(cd.offset) << cbytes_ref(cd.size) << cbytes_ref(cd.data_size);
    return os;
  }
};


class csav
{
public:
  std::filesystem::path filepath;
  csav_version ver;
  uint32_t uk0, uk1;
  std::string suk;

  serial_tree stree;
  std::shared_ptr<const node_t> root_node;

  // structures and systems
  CInventory                inventory;
  CCharacterCustomization   chtrcustom;
  CStatsPool                statspool;
  CStats                    stats;
  //CPSData                   psdata;

protected:
  bool load_stree(std::filesystem::path path);
  bool save_stree(std::filesystem::path path, bool dump_decompressed_data=false, bool ps4_weird_format=false);

public:
  bool open_with_progress(std::filesystem::path path, float& progress, bool test_reserialization=true)
  {
    progress = 0.0f;
    if (!load_stree(path))
      return false;
    progress = 0.2f;
    try_load_node_data_struct(inventory,  "inventory"                           , false); progress = 0.3f;
    try_load_node_data_struct(chtrcustom, "CharacetrCustomization_Appearances"  , false); progress = 0.4f;
    //try_load_node_data_struct(psdata,     "PSData"                            , test_reserialization); progress = 0.7f;
    //try_load_node_data_struct(stats,      "StatsSystem"                         , test_reserialization); progress = 0.8f;
    try_load_node_data_struct(statspool,  "StatPoolsSystem"                     , test_reserialization); progress = 1.0f;
    
    return true;
  }

  bool save_with_progress(std::filesystem::path path, float& progress, bool dump_decompressed_data=false, bool ps4_weird_format=false)
  {
    progress = 0.0f;

    try_save_node_data_struct(inventory,  "inventory");                           progress = 0.1f;
    try_save_node_data_struct(chtrcustom, "CharacetrCustomization_Appearances");  progress = 0.2f;
    //try_save_node_data_struct(psdata,     "PSData");                              progress = 0.5f;
    //try_save_node_data_struct(stats,      "StatsSystem");                         progress = 0.6f;
    try_save_node_data_struct(statspool,  "StatPoolsSystem");                     progress = 0.8f;
    
    if (!save_stree(path, dump_decompressed_data, ps4_weird_format))
      return false;
    progress = 1.0f;
    return true;
  }

protected:
  bool try_load_node_data_struct(node_serializable& var, std::string_view nodename, bool test_reserialization=false)
  {
    auto node = search_node(nodename);
    if (!node)
      return false;
    if (!var.from_node(node, ver))
      return false;

    if (test_reserialization)
    {
      auto new_node = var.to_node(ver);
      if (!new_node)
        return false;

      serial_tree stree1, stree2;

      {
        auto root = node_t::create_shared(node_t::root_node_idx, "root");
        root->nonconst().children_push_back(node);
        stree1.from_node(root, 4);
      }
      {
        auto root = node_t::create_shared(node_t::root_node_idx, "root");
        root->nonconst().children_push_back(new_node);
        stree2.from_node(root, 4);
      }

      if (stree1.nodedata.size() != stree2.nodedata.size()
        || std::memcmp(stree1.nodedata.data(), stree2.nodedata.data(), stree1.nodedata.size()))
      {
        std::ofstream ofs;
        ofs.open("dump1.bin");
        ofs.write(stree1.nodedata.data(), stree1.nodedata.size());
        ofs.close();
        ofs.open("dump2.bin");
        ofs.write(stree2.nodedata.data(), stree2.nodedata.size());
        ofs.close();

        throw std::runtime_error(fmt::format("reserialized {} differs from original", nodename));
      }
    }
    return true;
  }

  bool try_save_node_data_struct(node_serializable& var, std::string_view nodename)
  {
    auto node = search_node(nodename);
    if (!node)
      return false;
    auto new_node = var.to_node(ver);
    if (!new_node)
      return false;

    auto ncnode = std::const_pointer_cast<node_t>(node);
    ncnode->assign_children(new_node->children());
    ncnode->assign_data(new_node->data());

    return true;
  }


public:
  std::shared_ptr<const node_t> search_node(std::string_view name) const
  {
    if (root_node)
      return search_node(root_node, name);

    return nullptr;
  }

  std::shared_ptr<const node_t> search_node(const std::shared_ptr<const node_t>& node, std::string_view name) const
  {
    if (node->name() == name)
      return node;

    for (auto& c : node->children())
    {
      auto res = search_node(c, name);
      if (res) return res;
    }

    return nullptr;
  }
};

