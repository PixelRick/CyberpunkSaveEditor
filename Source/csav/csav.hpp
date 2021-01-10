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

// todo, make a dedicated struct for the compressed serial tree functionality
// loading systems isn't necessary to work on nodes only

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
  CGenericSystem            scriptables;

  CStatsPool                statspool;
  CStats                    stats;

  CPSData                   psdata;

  CGenericSystem            godmode;

  //CGenericSystem            scriptables;

protected:
  bool load_stree(std::filesystem::path path, bool dump_decompressed_data=false);
  bool save_stree(std::filesystem::path path, bool dump_decompressed_data=false, bool ps4_weird_format=false);

public:
  // reserialization test can only be done with file saved by the game
  // this is because although the order of the CProperties isn't important for the game
  // we don't want to keep the initial order for each object but rely on a standardized one (blueprint db)
  // the one the game uses
  bool open_with_progress(std::filesystem::path path, progress_t& progress, bool dump_decompressed_data=false, bool tree_only=false, bool test=true)
  {
    progress.value = 0.00f;
    if (!load_stree(path, dump_decompressed_data))
      return false;

    if (tree_only)
    {
      return true;
      progress.value = 1.f;
    }

    progress.value = 0.20f;

    progress.comment = "loading game classes definitions";
    CObjectBPList::get();
    progress.value = 0.25f;

    try_load_node_data_struct(inventory,    "inventory"                           , progress, 0.30f, test);
    try_load_node_data_struct(chtrcustom,   "CharacetrCustomization_Appearances"  , progress, 0.35f, test);

    try_load_node_data_struct(godmode,      "godModeSystem"                       , progress, 0.40f, test);

    try_load_node_data_struct(scriptables,  "ScriptableSystemsContainer"          , progress, 0.50f, test);
    try_load_node_data_struct(psdata,       "PSData"                              , progress, 0.80f, test);

    try_load_node_data_struct(stats,        "StatsSystem"                         , progress, 0.90f, test);
    try_load_node_data_struct(statspool,    "StatPoolsSystem"                     , progress, 1.00f, test);
    
    return true;
  }

  bool save_with_progress(std::filesystem::path path, progress_t& progress, bool dump_decompressed_data=false, bool ps4_weird_format=false)
  {
    progress.value = 0.00f;

    try_save_node_data_struct(inventory,    "inventory"                             );  progress.value = 0.10f;
    try_save_node_data_struct(chtrcustom,   "CharacetrCustomization_Appearances"    );  progress.value = 0.15f;

    try_save_node_data_struct(godmode,      "godModeSystem"                         );  progress.value = 0.20f;

    try_save_node_data_struct(scriptables,  "ScriptableSystemsContainer"            );  progress.value = 0.30f;
    try_save_node_data_struct(psdata,       "PSData"                                );  progress.value = 0.60f;

    try_save_node_data_struct(stats,        "StatsSystem"                           );  progress.value = 0.70f;
    try_save_node_data_struct(statspool,    "StatPoolsSystem"                       );  progress.value = 0.80f;
    

    if (!save_stree(path, dump_decompressed_data, ps4_weird_format))
      return false;
    progress.value = 1.00f;
    return true;
  }

protected:
  bool try_load_node_data_struct(node_serializable& var, std::string_view nodename, progress_t& progress, float end_progress, bool test=false)
  {
    auto node = search_node(nodename);
    if (!node)
      return false;

    bool ok = false;

    progress.comment.assign(fmt::format("Loading node {}", node->name()));
    if (load_node_data_struct(node, var))
    {
      if (test)
      {
        progress.value = 0.5f * (end_progress + progress.value);
        progress.comment.assign(fmt::format("Testing reserialization of node {}", node->name()));
        if (test_reserialize(node, var))
          ok = true;
      }
      else
      {
        ok = true;
      }
    }

    progress.value = end_progress;
    return ok;
  }

  bool test_reserialize(const std::shared_ptr<const node_t>& node, node_serializable& var)
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
      ofs.open(fmt::format("dump_{}_orig.bin", node->name()));
      ofs.write(stree1.nodedata.data(), stree1.nodedata.size());
      ofs.close();
      ofs.open(fmt::format("dump_{}_reserialized.bin", node->name()));
      ofs.write(stree2.nodedata.data(), stree2.nodedata.size());
      ofs.close();

      // it's easier to call this atm than the GUI's error box
      MessageBoxA(
        0,
        fmt::format(
          "Reserialized \"{}\" node differs from original\n"
          "\n"
          "If your save has been edited with an older version of CPSE,\n"
          "please make the game save it again.\n"
          "\n"
          "Otherwise, please open an issue.",
          node->name()
        ).c_str(), 
        "Reserialization test failed.",
        0);

      return false;
    }

    return true;
  }

  bool load_node_data_struct(const std::shared_ptr<const node_t>& node, node_serializable& var)
  {
    try
    {
      if (!var.from_node(node, ver))
        return false;
    }
    catch (std::exception& e)
    {
      MessageBoxA(0, fmt::format("couldn't load node {}\nreason: {}", node->name(), e.what()).c_str(), "error", 0);
      return false;
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

