#pragma once
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <numeric>

#define NULL_NODE_IDX -1
#define ROOT_NODE_IDX -2
#define BLOB_NODE_IDX -3

class node_t
  : public std::enable_shared_from_this<const node_t>
{
  friend class node_dataview;

  int32_t m_idx;
  const std::string m_name;
  std::vector<std::shared_ptr<const node_t>> m_children;
  std::vector<char> m_data;

protected:
  explicit node_t(int32_t idx, std::string name)
    : m_name(name)
  {
    if (idx < BLOB_NODE_IDX)
      idx = BLOB_NODE_IDX;
    m_idx = idx;
  }

public:
  static std::shared_ptr<const node_t> create_shared(int32_t idx, std::string name)
  {
    struct make_shared_enabler : public node_t {
      make_shared_enabler(int32_t idx, std::string name) : node_t(idx, name) {}
    };
    return std::make_shared<const make_shared_enabler>(idx, name);
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


// secure editing of node
class node_view
{
  static inline std::map<uint64_t, std::list<node_view*>> s_views;
  std::shared_ptr<node_t> m_node;

protected:
  node_view() = delete;

  explicit node_view(const std::shared_ptr<const node_t>& node)
    : m_node(std::const_pointer_cast<node_t>(node))
  {
    const uint64_t uid = (uint64_t)m_node.get();
    auto& list = s_views.emplace(uid, std::list<node_view*>()).first->second;
    list.emplace_back(this);
  }

  virtual ~node_view()
  {
    // erasing this from s_views
    const uint64_t uid = (uint64_t)m_node.get();
    auto& list = s_views.find(uid)->second;
    list.remove(this);
  }

public:
  std::shared_ptr<const node_t> node() const {
    return std::const_pointer_cast<const node_t>(m_node);
  }

  template <class Iter>
  void assign_node_data(Iter first, Iter last)
  {
    auto& buf = m_node->data();
    buf.assign(first, last);
  }

  void patch_node_data(size_t offset, size_t len, const char* srcbuf, size_t srclen)
  {
    auto& buf = m_node->data();
    auto it_start = buf.begin() + offset;
    if (len < srclen)
      buf.insert(it_start, srclen - len, 0);
    else
      buf.erase(it_start, it_start + len - srclen);
    std::copy(srcbuf, srcbuf + srclen, it_start);

    signal_dirty(offset, len, srclen);
  }

  const std::vector<char>& node_data_buffer() const { return m_node->data(); } const

  void signal_dirty(size_t offset, size_t len, size_t patch_len) const
  {
    const uint64_t uid = (uint64_t)m_node.get();
    for (auto& view : s_views.find(uid)->second)
      view->on_dirty(offset, len, patch_len);
  }

protected:
  virtual void on_dirty(size_t offset, size_t len, size_t patch_len) = 0;
};

