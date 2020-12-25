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
  : protected node_view
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
  static inline std::string s_error;
  bool m_window_opened = true;
  std::string m_window_name;

protected:
  node_editor(const std::shared_ptr<const node_t>& node)
    : node_view(node)
  {
    std::stringstream ss;
    ss << "node:" << node->name() << " idx:" << node->idx();
    m_window_name = ss.str();
  }

  ~node_editor() override {};

  bool m_dirty = false;
  bool m_has_changes = false;

private:
  void on_node_data_changed() override {
    m_dirty = true;
  }

public:
  std::shared_ptr<const node_t> node() const { return node_view::node(); }

  void draw_window(bool* p_open = nullptr)
  {
    m_window_opened = true;
    auto& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin(m_window_name.c_str(), &m_window_opened, ImGuiWindowFlags_AlwaysAutoResize))
    {
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
        if (ImGui::ButtonEx("reload", ImVec2(60, 0), ImGuiButtonFlags_PressedOnDoubleClick)) {
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
      if (!m_window_opened)
        ImGui::OpenPopup("Unsaved changes##node_editor");
      m_window_opened = true;
    }

    if (ImGui::BeginPopupModal("Unsaved changes##node_editor", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text("Unsaved changes, would you like to save ?");

      ImGui::Separator();
      if (ImGui::ButtonEx("NO", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0), ImGuiButtonFlags_PressedOnDoubleClick))
      {
        m_window_opened = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("YES", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
      {
        if (commit())
          m_window_opened = false;
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (p_open)
      *p_open = m_window_opened;
  }

  void draw_widget(const ImVec2& size = ImVec2(0, 0))
  {
    ImGui::PushID((void*)this);
    draw_impl(size);

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

    ImGui::PopID();
  }

  static void error(std::string_view msg)
  {
    s_error = msg;
    ImGui::OpenPopup("Error##node_editor");
  }

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

