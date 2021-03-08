#pragma once
#include <filesystem>
#include <vector>

#include <cpinternals/csav/node.hpp>
#include <cpinternals/csav/version.hpp>
#include <cpinternals/csav/serial_tree.hpp>

namespace cp::csav {

struct node_tree
{
  using node_type = node_t;
  using shared_node_type = std::shared_ptr<const node_t>;

  version& ver()
  {
    return m_ver;
  }

  const version& ver() const
  {
    return m_ver;
  }

  op_status load(std::filesystem::path path);

  // This one makes a backup!
  op_status save(std::filesystem::path path);

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

  std::vector<serial_node_desc> original_descs;
  shared_node_type root;

protected:

  void serialize_in(streambase& ar);
  void serialize_out(streambase& ar);

  version m_ver;
};

} // namespace cp::csav


namespace cp {

using csav_tree = csav::node_tree;

} // namespace cp

