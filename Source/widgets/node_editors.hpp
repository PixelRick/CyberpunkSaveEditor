#pragma once

#include <map>

#include "node_editors/node_editor.hpp"
#include "node_editors/hexedit.hpp"
#include "node_editors/inventory.hpp"

// this keeps one window per node max
class node_editors_wndmgr
{
public:
  static node_editors_wndmgr& get()
  {
    static node_editors_wndmgr s;
    return s;
  }

  node_editors_wndmgr(const node_editors_wndmgr&) = delete;
  node_editors_wndmgr& operator=(const node_editors_wndmgr&) = delete;

private:
  node_editors_wndmgr() = default;
  ~node_editors_wndmgr() = default;

  std::map<uint64_t, std::shared_ptr<node_editor>> m_opened_editors;

public:
  std::shared_ptr<node_editor> get_opened_editor(const std::shared_ptr<const node_t>& node) const
  {
    const uint64_t nid = (uint64_t)node.get();
    auto it = m_opened_editors.find(nid);
    if (it != m_opened_editors.end())
      return it->second;
    return nullptr;
  }

  bool has_opened_editor(const std::shared_ptr<const node_t>& node) const
  {
    return get_opened_editor(node) != nullptr;
  }

  std::shared_ptr<node_editor> open_editor(const std::shared_ptr<const node_t>& node)
  {
    const uint64_t nid = (uint64_t)node.get();
    auto it = m_opened_editors.find(nid);
    if (it == m_opened_editors.end())
    {
      auto new_editor = node_editor::create(node);
      m_opened_editors[nid] = new_editor;
      return new_editor;
    }
    return it->second;
  }

  void draw_editors()
  {
    uint64_t id = (uint64_t)this;
    for (auto it = m_opened_editors.begin(); it != m_opened_editors.end();)
    {
      bool opened = true;
      ImGui::PushID(++id);
      it->second->draw_window(&opened);
      if (!opened)
        it = m_opened_editors.erase(it);
      else
        ++it;
      ImGui::PopID();
    }
  }
};


