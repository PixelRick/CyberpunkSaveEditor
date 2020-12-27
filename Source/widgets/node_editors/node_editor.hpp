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

class node_editor
  : public node_listener_t
{
  // inlined factory -start-

  template <class Derived, std::enable_if_t<std::is_base_of_v<node_editor, Derived>, int> = 0>
  static inline std::shared_ptr<node_editor> create(const std::shared_ptr<const node_t>& node)
  {
    struct make_shared_enabler : public Derived {
      make_shared_enabler(const std::shared_ptr<const node_t>& node) : Derived(node) {}
    };
    return std::dynamic_pointer_cast<node_editor>(std::make_shared<make_shared_enabler>(node));
  }

  using bound_create_t = decltype(std::bind(&create<node_editor>, std::placeholders::_1));
  using factory_map_t = std::map<std::string, 
    std::function<std::shared_ptr<node_editor>(const std::shared_ptr<const node_t>&)>>;

  static inline factory_map_t s_factory_map;

public:
  template <class Derived, std::enable_if_t<std::is_base_of_v<node_editor, Derived>, int> = 0>
  static inline void
  factory_register_for_node_name(const std::string& node_name)
  {
    struct make_shared_enabler : public Derived {
      make_shared_enabler(const std::shared_ptr<const node_t>& node) : Derived(node) {}
    };
    s_factory_map[node_name] = std::bind(&create<make_shared_enabler>, std::placeholders::_1);
  }

  static inline std::shared_ptr<node_editor>
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
  std::string m_window_title;
  bool m_window_take_focus = false;
  bool m_window_has_focus = true;
  bool m_window_opened = false;

protected:
  node_editor(const std::shared_ptr<const node_t>& node)
    : m_weaknode(node)
  {
    std::stringstream ss;
    ss << "node:" << node->name() << " idx:" << node->idx();
    m_window_title = ss.str();
    node->add_listener(this);
  }

  ~node_editor() override
  {
    auto node = m_weaknode.lock();
    if (node)
      node->remove_listener(this);
  };

  bool m_dirty = false;
  bool m_has_changes = false;
  bool m_is_drawing = false; // to filter events

protected:
  void on_node_event(const std::shared_ptr<const node_t>& node, node_event_e evt) override
  {
    if (m_is_drawing)
      m_has_changes = true;
    else
      m_dirty = true;
  }

public:
  bool has_changes() const { return m_has_changes; }

  std::shared_ptr<const node_t> node() const { return m_weaknode.lock(); }
  std::shared_ptr<node_t> ncnode() { return std::const_pointer_cast<node_t>(m_weaknode.lock()); }

  void focus_window()
  {
    m_window_take_focus = true;
  }

  bool has_focus() const { return m_window_has_focus; }

  void open_window()
  {
    m_window_opened = true;
  }

  bool has_opened_window() const { return m_window_opened; }

  void draw_window()
  {
    if (!m_window_opened)
      return;

    if (m_window_take_focus)
    {
      ImGui::SetNextWindowFocus();
      m_window_take_focus = false;
    }

    auto node = this->node();
    if (!node)
    {
      m_window_opened = false;
      return;
    }

    bool opened = true;
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

    if (ImGui::Begin(m_window_title.c_str(), &opened, ImGuiWindowFlags_AlwaysAutoResize))
    {
      m_window_has_focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
      if (m_dirty)
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
          reload();
        }
        ImGui::SameLine();
        if (ImGui::Button("ok", ImVec2(40, 0))) {
          m_dirty = false;
        }

        ImGui::PopStyleColor(3);
      }

      draw_widget();
    }
    ImGui::End();

    if (m_has_changes)
    {
      if (!opened)
        ImGui::OpenPopup("Unsaved changes##node_editor");
      opened = true;
    }

    if (ImGui::BeginPopupModal("Unsaved changes##node_editor", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text("Unsaved changes, would you like to save ?");

      float button_width = ImGui::GetContentRegionAvail().x * 0.25f;
      ImGui::Separator();
      if (ImGui::Button("NO", ImVec2(button_width, 0)))
      {
        opened = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("YES", ImVec2(button_width, 0)))
      {
        if (commit())
          opened = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("CANCEL", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
      {
        opened = true;
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    m_window_opened = opened;
  }

  void draw_widget(const ImVec2& size = ImVec2(0, 0))
  {
    ImGui::PushID((void*)this);

    auto node = m_weaknode.lock();
    if (!node)
    {
      ImGui::Text("node no longer exists");
    }
    else
    {
      m_is_drawing = true;
      draw_impl(size);
      m_is_drawing = false;

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

      if (ImGui::Button("commit changes##node_editor", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
        commit();
      ImGui::SameLine();
      if (ImGui::Button("discard and reload##node_editor", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
        reload();
    }

    ImGui::PopID();
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
    m_has_changes = false;
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
    m_has_changes = false;
    m_dirty = false;
    return true;
  }

private:
  virtual void draw_impl(const ImVec2& size) = 0;
  virtual bool commit_impl() = 0;
  virtual bool reload_impl() = 0;
};

/*  starter template
// ------------------

class node_xxxeditor
  : public node_editor
{
  // attrs

public:
  node_xxxeditor(const std::shared_ptr<const node_t>& node)
    : node_editor(node)
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


// the idea here is to have a single editor per node
// and editor state is thus the same between opened window and widget
// + you could make all widgets have a "to window" button..

class node_editors_mgr
{
public:
  static node_editors_mgr& get()
  {
    static node_editors_mgr s;
    return s;
  }

  node_editors_mgr(const node_editors_mgr&) = delete;
  node_editors_mgr& operator=(const node_editors_mgr&) = delete;

private:
  node_editors_mgr() = default;
  ~node_editors_mgr() = default;

  std::map<
    std::weak_ptr<const node_t>,
    std::shared_ptr<node_editor>,
    std::owner_less<std::weak_ptr<const node_t>>
  > m_editors;

public:
  std::shared_ptr<node_editor> find_editor(const std::shared_ptr<const node_t>& node) const
  {
    auto it = m_editors.find(node);
    if (it != m_editors.end())
      return it->second;
    return nullptr;
  }

  bool has_editor(const std::shared_ptr<const node_t>& node) const
  {
    const uint64_t nid = (uint64_t)node.get();
    return find_editor(node) != nullptr;
  }

  std::shared_ptr<node_editor> get_editor(const std::shared_ptr<const node_t>& node)
  {
    auto it = m_editors.find(node);
    if (it != m_editors.end())
      return it->second;
    
    auto editor = node_editor::create(node);
    if (editor)
      m_editors[node] = editor;

    return editor;
  }

  void draw_editors()
  {
    uint64_t id = (uint64_t)this;
    for (auto it = m_editors.begin(); it != m_editors.end();)
    {
      auto n = it->first.lock();
      if (!n)
      {
        it = m_editors.erase(it);
      }
      else
      {
        ImGui::PushID((int)++id);
        it->second->draw_window();
        ImGui::PopID();

        ++it;
      }
    }
  }
};

