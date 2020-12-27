#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>

#include "AppLib/IApp.h"
#include "cserialization/node.hpp"

#define NODE_EDITOR__DEFAULT_LEAF_EDITOR_NAME "<default_editor>"


// it should be drawn only once per frame
// ensuring that isn't easy


class node_editor_widget
  : public node_listener_t
{
  friend class node_editor_window;

  // inlined factory -start-

  template <class Derived, std::enable_if_t<std::is_base_of_v<node_editor_widget, Derived>, int> = 0>
  static inline std::shared_ptr<node_editor_widget> create(const std::shared_ptr<const node_t>& node)
  {
    return std::dynamic_pointer_cast<node_editor_widget>(std::make_shared<Derived>(node));
  }

  using bound_create_t = decltype(std::bind(&create<node_editor_widget>, std::placeholders::_1));
  using factory_map_t = std::map<std::string, 
    std::function<std::shared_ptr<node_editor_widget>(const std::shared_ptr<const node_t>&)>>;

  static inline factory_map_t s_factory_map;

public:
  template <class Derived, std::enable_if_t<std::is_base_of_v<node_editor_widget, Derived>, int> = 0>
  static inline void
  factory_register_for_node_name(const std::string& node_name)
  {
    s_factory_map[node_name] = std::bind(&create<Derived>, std::placeholders::_1);
  }

  static inline std::shared_ptr<node_editor_widget>
  create(const std::shared_ptr<const node_t>& node)
  {
    auto it = s_factory_map.find(node->name());
    if (it == s_factory_map.end() && node->is_leaf())
      it = s_factory_map.find(NODE_EDITOR__DEFAULT_LEAF_EDITOR_NAME);
    if (it != s_factory_map.end())
      return it->second(node);
    return nullptr;
  }

  // inlined factory -end-

private:
  std::weak_ptr<const node_t> m_weaknode;

public:
  node_editor_widget(const std::shared_ptr<const node_t>& node)
    : m_weaknode(node)
  {
    node->add_listener(this);
  }

  ~node_editor_widget() override
  {
    auto node = m_weaknode.lock();
    if (node)
      node->remove_listener(this);
  };

protected:
  bool m_dirty = false;
  bool m_has_unsaved_changes = false;
  bool m_is_drawing = false; // to filter events

  void on_node_event(const std::shared_ptr<const node_t>& node, node_event_e evt) override
  {
    if (m_is_drawing)
      m_has_unsaved_changes = true;
    else
      m_dirty = true;
  }

public:
  bool has_changes() const { return m_has_unsaved_changes; }
  bool is_dirty() const { return m_dirty; }

  std::shared_ptr<const node_t> node() const { return m_weaknode.lock(); }
  std::shared_ptr<node_t> ncnode() { return std::const_pointer_cast<node_t>(m_weaknode.lock()); }

  void draw_widget(const ImVec2& size = ImVec2(0, 0), bool with_save_buttons=true)
  {
    ImGui::PushID((void*)this);

    auto node = m_weaknode.lock();
    if (!node)
    {
      ImGui::Text("node no longer exists");
    }
    else
    {
      if (with_save_buttons)
      {
        if (ImGui::Button("save##node_editor", ImVec2(ImGui::GetContentRegionAvail().x * 0.6f, 0)))
          commit();
        ImGui::SameLine();
        if (ImGui::Button("discard and reload##node_editor", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
          reload();
      }

      draw_content(size);

      if (ImGui::BeginPopupModal("Error##node_editor", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
      {
        ImGui::Text("%s", s_error.c_str());

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        {
          ImGui::CloseCurrentPopup();
          s_error = "";
        }

        ImGui::EndPopup();
      }
    }

    ImGui::PopID();
  }

protected:
  void draw_content(const ImVec2& size = ImVec2(0, 0))
  {
    m_is_drawing = true;
    draw_impl(size);
    m_is_drawing = false;
  }

private:
  static inline std::string s_error;

protected:
  static void error(std::string_view msg)
  {
    s_error = msg;
    ImGui::OpenPopup("Error##node_editor");
  }

public:
  bool commit()
  {
    if (!commit_impl())
    {
      error(s_error.empty() ? "commit failed" : s_error);
      return false;
    }
    m_has_unsaved_changes = false;
    m_dirty = false;
    return true;
  }

  bool reload()
  {
    if (!reload_impl())
    {
      error(s_error.empty() ? "reload failed" : s_error);
      return false;
    }
    m_has_unsaved_changes = false;
    m_dirty = false;
    return true;
  }

private:
  // should return true if data has been modified
  virtual void draw_impl(const ImVec2& size) = 0;
  virtual bool commit_impl() = 0;
  virtual bool reload_impl() = 0;
};

/*  starter template
// ------------------

class node_xxxeditor
  : public node_editor_widget
{
  // attrs

public:
  node_xxxeditor(const std::shared_ptr<const node_t>& node)
    : node_editor_widget(node)
  {
    reload();
  }

public:
  bool commit() override
  {
  }

  bool reload() override
  {
  }

protected:
  void draw_impl(const ImVec2& size) override
  {
  }

  void on_dirty(size_t offset, size_t len, size_t patch_len) override
  {
  }
};

*/

// onlyn ode_editor_windows_mgr can instantiate these
class node_editor_window
{
  friend class node_editor_windows_mgr;
  struct create_tag {};

private:
  std::shared_ptr<node_editor_widget> editor;
  std::string m_window_title;
  bool m_take_focus = false;
  bool m_has_focus = false;
  bool m_opened = false;
  bool m_dirty_aknowledged = false;

public:
  node_editor_window(create_tag&&, const std::shared_ptr<const node_t> node)
  {
    std::stringstream ss;
    ss << "node:" << node->name();
    if (node->is_cnode())
      ss << " idx:" << node->idx();
    ss << "##node_editor_window_" << (uint64_t)node.get();
    m_window_title = ss.str();
    editor = node_editor_widget::create(node);
  }

  static std::shared_ptr<node_editor_window> create(const std::shared_ptr<const node_t> node)
  {
    return std::make_shared<node_editor_window>(create_tag{}, node);
  }

public:
  void open() {
    m_opened = true;
  }

  bool is_opened() const { return m_opened; }

  void take_focus() {
    m_take_focus = true;
  }

  bool has_focus() const { return m_has_focus; }

protected:
  void draw()
  {
    if (!m_opened)
      return;

    if (m_take_focus)
    {
      ImGui::SetNextWindowFocus();
      m_take_focus = false;
    }

    auto node = editor->node();
    if (!node)
    {
      m_opened = false;
      return;
    }

    auto& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    std::stringstream ss;
    uint64_t nid = (uint64_t)node.get();
    ss << "node:" << node->name();
    if (node->is_cnode())
      ss << " idx:" << node->idx();
    ss << "##" << nid;
    m_window_title = ss.str();

    if (ImGui::Begin(m_window_title.c_str(), &m_opened, ImGuiWindowFlags_AlwaysAutoResize))
    {
      m_has_focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

      if (editor->is_dirty())
      {
        if (!m_dirty_aknowledged)
        {
          //ImGui::PushStyleColor
          ImVec4 color_red = ImColor::HSV(0.f, 1.f, 0.7f, 1.f).Value;
          // ImGuiCol_ModalWindowDimBg
          ImGui::PushStyleColor(ImGuiCol_Text, color_red);
          ImGui::Text("content modified outside of this editor", ImVec2(ImGui::GetContentRegionAvail().x - 100, 0));
          ImGui::PopStyleColor();

          ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.f, 0.6f, 0.6f).Value);
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.f, 0.7f, 0.7f).Value);
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.f, 0.8f, 0.8f).Value);

          ImGui::SameLine();
          if (ImGui::Button("reload", ImVec2(60, 0))) {
            editor->reload();
          }
          ImGui::SameLine();
          if (ImGui::Button("ok", ImVec2(40, 0))) {
            m_dirty_aknowledged = false;
          }

          ImGui::PopStyleColor(3);
        }
      }
      else
      {
        m_dirty_aknowledged = false;
      }

      editor->draw_widget();
    }
    ImGui::End();

    if (editor->has_changes())
    {
      if (!m_opened)
        ImGui::OpenPopup("Unsaved changes##node_editor");
      m_opened = true;
    }

    if (ImGui::BeginPopupModal("Unsaved changes##node_editor", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text("Unsaved changes, would you like to save ?");

      float button_width = ImGui::GetContentRegionAvail().x * 0.25f;
      ImGui::Separator();
      if (ImGui::Button("NO", ImVec2(button_width, 0)))
      {
        m_opened = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("YES", ImVec2(button_width, 0)))
      {
        if (editor->commit())
          m_opened = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("CANCEL", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
      {
        m_opened = true;
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }
  }
};


class node_editor_windows_mgr
{
public:
  static node_editor_windows_mgr& get()
  {
    static node_editor_windows_mgr s;
    return s;
  }

  node_editor_windows_mgr(const node_editor_windows_mgr&) = delete;
  node_editor_windows_mgr& operator=(const node_editor_windows_mgr&) = delete;

private:
  node_editor_windows_mgr() = default;
  ~node_editor_windows_mgr() = default;

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

    auto window = node_editor_window::create(node);
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






