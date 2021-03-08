#pragma once
#include <filesystem>
#include <vector>

#include "node.hpp"
#include "csav_version.hpp"
#include "serial_tree.hpp"

namespace cp {
namespace csav {

struct node_tree
{
  using node_type = node_t;
  using shared_node_type = std::shared_ptr<const node_t>;

  friend streambase& operator<<(streambase& ar, node_tree& x)
  {
    if (ar.is_reader())
    {
      x.serialize_in(ar);
    }
    else
    {
      x.serialize_out(ar);
    }
  }

  op_status load(std::filesystem::path path);

  // This one makes a backup!
  op_status save(std::filesystem::path path);

  csav_version version;
  std::vector<serial_node_desc> original_descs;
  shared_node_type root;

protected:
  void serialize_in(streambase& ar);
  void serialize_out(streambase& ar);
};

} // namespace csav

using csav_tree = csav::node_tree;

} // namespace cp

