#pragma once
#include <string>
#include <vector>
#include <memory>
#include <numeric>

#define NULL_NODE_IDX -1
#define ROOT_NODE_IDX -2
#define BLOB_NODE_IDX -3

class node_t
  : std::enable_shared_from_this<node_t>
{
  friend class node_dataview;

  int32_t m_idx;
  std::string m_name;
  std::vector<std::shared_ptr<const node_t>> m_children;
  std::vector<char> m_data;

public:
  explicit node_t(int32_t idx, std::string name)
    : m_name(name)
  {
    if (idx < BLOB_NODE_IDX)
      idx = BLOB_NODE_IDX;
    m_idx = idx;
  }

public:
  node_t& nonconst() const { return const_cast<node_t&>(*this); }

  int32_t idx() const { return m_idx; }
  void idx(int32_t idx) { m_idx = idx; }

  std::string name() const { return m_name; }

  const std::vector<std::shared_ptr<const node_t>>&
  children() const { return m_children; }

  std::vector<std::shared_ptr<const node_t>>&
  children() { return m_children; }

  const std::vector<char>&
  data() const { return m_data; }

  std::vector<char>&
  data() { return m_data; }

  bool is_root() const { return m_idx == ROOT_NODE_IDX; }
  bool is_blob() const { return m_idx == BLOB_NODE_IDX; }
  bool is_cnode() const { return m_idx >= 0; }

  bool has_children() const { return !m_children.empty(); }

public:
  size_t calcsize() const
  {
    size_t base_size = m_data.size() + (is_cnode() ? 4 : 0);
    return std::accumulate(
      m_children.begin(), m_children.end(), base_size,
      [](size_t cnt, auto& node){ return cnt + node->calcsize(); }
    );
  }

  uint32_t treecount() const
  {
    if (!is_blob())
    {
      return std::accumulate(
        m_children.begin(), m_children.end(), is_root() ? 0 : (uint32_t)1,
        [](uint32_t cnt, auto& node){ return cnt + node->treecount(); }
      );
    }
    return 0;
  }
};


class node_dataview
{
protected:
  std::shared_ptr<const node_t> m_node;

public:
  const std::shared_ptr<const node_t>&
  node() const { return m_node; }

public:
  virtual void commit() = 0;
  virtual void reload() = 0;
};

