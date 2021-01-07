#pragma once
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <numeric>
#include <sstream>
#include <set>
#include "utils.hpp"
#include "csav_version.hpp"

class node_t;

enum class node_event_e
{
  data_update,
  children_update,
  subtree_update,
};

struct node_listener_t
{
  virtual ~node_listener_t() = default;
  virtual void on_node_event(const std::shared_ptr<const node_t>& node, node_event_e evt) = 0;
};

class node_t
  : public std::enable_shared_from_this<const node_t>
  , public node_listener_t
{
  struct create_tag {};

public:
  static const int32_t null_node_idx = -1;
  static const int32_t root_node_idx = -2;
  static const int32_t blob_node_idx = -3;

private:
  int32_t           m_idx;
  const std::string m_name;
  std::vector<char> m_data;
  std::vector<std::shared_ptr<const node_t>> m_children;

public:
  explicit node_t(create_tag&&, int32_t idx, std::string name)
    : m_name(name)
  {
    if (idx < blob_node_idx)
      idx = blob_node_idx;
    m_idx = idx;
  }

  ~node_t()
  {
    for (auto& c : m_children)
      c->remove_listener(this);
  }

  static std::shared_ptr<const node_t>
  create_shared(int32_t idx, std::string name)
  {
    return std::make_shared<const node_t>(create_tag{}, idx, name);
  }

  template <class Iter>
  static std::shared_ptr<const node_t>
  create_shared_blob(Iter first, Iter last)
  {
    auto node = node_t::create_shared(node_t::blob_node_idx, "datablob");
    node->nonconst().assign_data(first, last);
    return node;
  }

  static std::shared_ptr<const node_t>
  create_shared_blob(const char* nodedata, uint32_t start_offset, uint32_t end_offset)
  {
    return create_shared_blob(nodedata + start_offset, nodedata + end_offset);
  }

public:

  int32_t idx() const       { return m_idx; }
  void    idx(int32_t idx)  { m_idx = idx; }

  // works on null pointers
  std::string name() const
  {
    if (!this)
      return "nullptr";
    return m_name;
  }

  const std::vector<std::shared_ptr<const node_t>>&
  children() const { return m_children; }

  const std::vector<char>&
  data() const { return m_data; }

  bool has_children() const { return !m_children.empty(); }

  bool is_root()  const { return m_idx == root_node_idx; }
  bool is_blob()  const { return m_idx == blob_node_idx; }
  bool is_cnode() const { return m_idx >= 0; }
  bool is_leaf()  const { return !has_children(); }

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

  node_t& nonconst() const { return const_cast<node_t&>(*this); }

  std::shared_ptr<const node_t> deepcopy() const
  {
    // not cycle-safe, but shouldn't happen..
    auto new_node = create_shared(0, name());
    auto& nc = new_node->nonconst();
    for (auto& c : m_children)
      nc.m_children.push_back(c->deepcopy());
    nc.m_data = m_data;
    return new_node;
  }

public: 
  // non const setters

  template <class Iter>
  void assign_data(Iter first, Iter last)
  {
    m_data.assign(first, last);
    post_node_event(node_event_e::data_update);
  }

  void assign_data(const std::vector<char>& buf)
  {
    assign_data(buf.begin(), buf.end());
  }

  template <class Iter>
  void assign_children(Iter first, Iter last)
  {
    for (auto& c : m_children)
      c->remove_listener(this);
    m_children.assign(first, last);
    for (auto& c : m_children)
      c->add_listener(this);
    post_node_event(node_event_e::children_update);
  }

  void assign_children(const std::vector<std::shared_ptr<const node_t>>& children)
  {
    assign_children(children.begin(), children.end());
  }

  template <class Iter>
  void children_push_back(const std::shared_ptr<const node_t>& node)
  {
    m_children.push_back(node);
    node->add_listener(this);
    post_node_event(node_event_e::children_update);
  }

protected:
  std::set<node_listener_t*> m_listeners;

  void post_node_event(node_event_e evt) const
  {
    std::set<node_listener_t*> listeners = m_listeners;
    for (auto& l : listeners) {
      l->on_node_event(shared_from_this(), evt);
    }
  }

  void on_node_event(const std::shared_ptr<const node_t>& node, node_event_e evt) override
  {
    post_node_event(node_event_e::subtree_update);
  }

public:
  // provided as const for ease of use

  void add_listener(node_listener_t* listener) const
  {
    auto& listeners = nonconst().m_listeners;
    listeners.insert(listener);
  }

  void remove_listener(node_listener_t* listener) const
  {
    auto& listeners = nonconst().m_listeners;
    listeners.erase(listener);
  }
};

// only to read at node level
// buffer and position in the istream is only relevant between child nodes
class node_reader
  : public std::istream
{
  std::shared_ptr<const node_t> m_node;
  vector_istreambuf m_sbuf;
  size_t m_cur_idx;
  csav_version m_ver;
  bool m_missed_data = false;

public:
  explicit node_reader(const std::shared_ptr<const node_t>& root, const csav_version& version)
    : m_node(root), m_ver(version), m_cur_idx(0), std::istream(&m_sbuf)
  {
    this->exceptions(std::ios::failbit | std::ios::badbit);
    auto blob = current_blob();
    if (blob)
      m_sbuf = vector_istreambuf(blob->data());
  }

  virtual ~node_reader() = default;

  const csav_version& version() const { return m_ver; }

  bool has_missed_data() { return m_missed_data; }

  void clear(std::ios::iostate state = std::ios::goodbit)
  {
    std::istream::clear(state);
    m_missed_data = false;
  }

protected:
  std::shared_ptr<const node_t> current_blob()
  {
    // the node is the blob (leaf)
    if (m_node->is_leaf())
      return m_cur_idx == 0 ? m_node : nullptr;

    if (m_cur_idx >= m_node->children().size())
      return nullptr;

    const auto& cur_node = m_node->children()[m_cur_idx];
    if (cur_node->is_blob())
      return cur_node;

    return nullptr;
  }

  void seek_past_current_blob_if_any()
  {
    const auto& blob = current_blob();
    if (!blob)
      return;

    size_t pos = tellg();
    if (pos != blob->data().size())
      m_missed_data = true;

    m_sbuf = {};
    m_cur_idx++;
  }

public:
  std::shared_ptr<const node_t> read_child(std::string_view name)
  {
    seek_past_current_blob_if_any();

    if (m_cur_idx >= m_node->children().size())
      return nullptr;

    const auto& child_node = m_node->children()[m_cur_idx];
    if (child_node->name() != name)
      return nullptr;
    m_cur_idx++;

    const auto& blob = current_blob();
    if (blob)
      m_sbuf = vector_istreambuf(blob->data());

    return child_node;
  }

  void skip(size_t len)
  {
    const auto& blob = current_blob();
    if (blob)
      seekg(len, std::ios::cur);
  }

  bool at_end()
  {
    if (m_node->is_leaf())
      return m_cur_idx > 0 || (tellg() == m_node->data().size());

    const size_t childcnt = m_node->children().size();
    const auto& blob = current_blob();
    if (blob)
    {
      if (tellg() != blob->data().size())
        return false;
      
      if (m_cur_idx == childcnt - 1)
        return true;
    }

    return m_cur_idx >= childcnt;
  }
};

// only to write at node level
// does not actually modify input node until finalize() is called
// buffer and position in the ostringstream is only relevant between child nodes
class node_writer
  : public std::ostringstream
{
  std::vector<std::shared_ptr<const node_t>> m_new_children;
  csav_version m_ver;

public:
  explicit node_writer(const csav_version& ver)
    : m_ver(ver) {}

  virtual ~node_writer() = default;

  const csav_version& version() const { return m_ver; }

protected:
  void blobize_pending_data_if_any()
  {
    auto data = str();
    if (data.size())
    {
      m_new_children.push_back(
        node_t::create_shared_blob(data.begin(), data.end()));
    }
    str("");
  }
  
public:
  void write_child(const std::shared_ptr<const node_t>& node)
  {
    blobize_pending_data_if_any();
    m_new_children.push_back(node);
  }

  void pad(size_t len)
  {
    *this << std::string(len, 0);
  }

  std::shared_ptr<const node_t> finalize(std::string name)
  {
    auto node = node_t::create_shared(0, name);
    finalize_in(node->nonconst());
    return node;
  }

  void finalize_in(node_t& node)
  {
    if (m_new_children.size())
      blobize_pending_data_if_any();
    // there is still pending data only if there are no children
    auto data = str();
    node.assign_data(data.begin(), data.end());
    node.assign_children(m_new_children.begin(), m_new_children.end());
  }
};

struct node_serializable
{
  virtual std::string node_name() const = 0;

  bool from_node(const std::shared_ptr<const node_t>& node, const csav_version& version)
  {
    return from_node_impl(node, version);
  }

  std::shared_ptr<const node_t> to_node(const csav_version& version) const
  {
    return to_node_impl(version);
  }

private:
  virtual bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) = 0;
  virtual std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const = 0;
};

