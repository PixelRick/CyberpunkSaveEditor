#pragma once
#include "node_editors.hpp"

class hexeditor_windows_mgr
{
public:
  static hexeditor_windows_mgr& get()
  {
    static hexeditor_windows_mgr s;
    return s;
  }

  hexeditor_windows_mgr(const hexeditor_windows_mgr&) = delete;
  hexeditor_windows_mgr& operator=(const hexeditor_windows_mgr&) = delete;

private:
  hexeditor_windows_mgr() = default;
  ~hexeditor_windows_mgr() = default;

private:
  std::map<
    std::weak_ptr<const node_t>,
    std::shared_ptr<node_editor_window>,
    std::owner_less<std::weak_ptr<const node_t>>
  > m_windows;

public:
  node_editor_window* find_window(const std::shared_ptr<const node_t>& node) const
  {
    auto it = m_windows.find(node);
    if (it != m_windows.end())
      return it->second.get();
    return nullptr;
  }

  node_editor_window* open_window(const std::shared_ptr<const node_t>& node, bool take_focus = false)
  {
    if (!node)
      return nullptr;

    auto it = m_windows.find(node);
    if (it != m_windows.end())
    {
      auto window = it->second;
      window->open();
      if (take_focus)
        window->take_focus();
      return window.get();
    }

    auto ed = std::make_shared<node_hexeditor>(node);
    auto window = std::make_shared<node_editor_window>(ed);
    if (window)
    {
      window->open();
      m_windows[node] = window;
    }

    return window.get();
  }

  void draw_windows()
  {
    uint64_t id = (uint64_t)this;
    for (auto it = m_windows.begin(); it != m_windows.end();)
    {
      auto n = it->first.lock();
      if (!n || !it->second) {
        m_windows.erase(it++);
      } else {
        (it++)->second->draw();
      }
    }
  }
};

