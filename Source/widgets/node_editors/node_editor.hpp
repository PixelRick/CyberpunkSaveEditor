#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>

#include "AppLib/IApp.hpp"
#include "cpinternals/csav/node.hpp"
#include "cpinternals/csav/csav_version.hpp"
#include "fmt/format.h"

#define NODE_EDITOR__DEFAULT_LEAF_EDITOR_NAME "<default_editor>"


// it should be drawn only once per frame
// ensuring that isn't easy


class node_editor_widget
  : public cp::csav::node_listener_t
{
  friend class node_editor_window;

private:
  std::weak_ptr<const cp::csav::node_t> m_weaknode;
  cp::csav::csav_version m_version;

public:
  node_editor_widget(const std::shared_ptr<const cp::csav::node_t>& node, const cp::csav::csav_version& version)
    : m_weaknode(node), m_version(version)
  {
    node->add_listener(this);
  }

  ~node_editor_widget() override
  {
    auto node = m_weaknode.lock();
    if (node)
      node->remove_listener(this);
  };

  const cp::csav::csav_version& version() const { return m_version; }

private:
  bool m_is_drawing = false; // to filter events
  bool m_dirty = false;
  bool m_has_unsaved_changes = false;

protected:
  void on_node_event(const std::shared_ptr<const cp::csav::node_t>& node, cp::csav::node_event_e evt) override
  {
    if (m_is_drawing)
      m_has_unsaved_changes = true;
    else
      m_dirty = true;
  }

public:
  bool has_changes() const { return m_has_unsaved_changes; }
  bool is_dirty() const { return m_dirty; }

  bool alive() const { return !!node(); }

  std::string node_name() const
  {
    auto n = node();
    if (n) return n->name();
    return "dead_node";
  }

  std::shared_ptr<const cp::csav::node_t> node() const { return m_weaknode.lock(); }
  std::shared_ptr<cp::csav::node_t> ncnode() { return std::const_pointer_cast<cp::csav::node_t>(m_weaknode.lock()); }

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
        if (m_has_unsaved_changes)
        {
          { scoped_imgui_button_hue _sibh(0.2f);
            if (ImGui::Button("commit changes##node_editor", ImVec2(160, 22)))
              commit();
          }
          ImGui::SameLine();
          { scoped_imgui_button_hue _sibh(0.0f);
            if (ImGui::Button("revert changes##node_editor", ImVec2(160, 22)))
              reload();
          }
        }
        else
        {
          ImGui::ButtonEx("commit changes##node_editor", ImVec2(160, 22), ImGuiButtonFlags_Disabled);
          ImGui::SameLine();
          ImGui::ButtonEx("revert changes##node_editor", ImVec2(160, 22), ImGuiButtonFlags_Disabled);
        }
      }

      draw_content(size);
    }

    ImGui::PopID();
  }

  // todo: centralize the error popups of the whole app somewhere
  static void draw_popups()
  {
    if (s_errors.size())
      ImGui::OpenPopup("Error##node_editor");

    // Always center this window when appearing
    ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Error##node_editor", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
      for (auto& l : s_errors)
      ImGui::Text("%s", l.c_str());

      ImGui::Separator();
      if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
      {
        ImGui::CloseCurrentPopup();
        s_errors.clear();
      }

      ImGui::EndPopup();
    }
  };

protected:
  void draw_content(const ImVec2& size = ImVec2(0, 0))
  {
    m_is_drawing = true;
    m_has_unsaved_changes |= draw_impl(size);
    m_is_drawing = false;
  }

private:
  static inline std::vector<std::string> s_errors;

protected:
  void error(std::string_view msg)
  {
    s_errors.push_back(fmt::format("node {} : {}", m_weaknode.lock()->name(), msg));
  }

public:
  bool commit()
  {
    if (!commit_impl())
    {
      error("commit failed");
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
      error("reload failed");
      return false;
    }
    m_has_unsaved_changes = false;
    m_dirty = false;
    return true;
  }

private:
  // should return true if data has been modified
  virtual bool draw_impl(const ImVec2& size) = 0;
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
  friend class hexeditor_windows_mgr;

private:
  std::shared_ptr<node_editor_widget> editor;
  std::string m_window_title;
  bool m_take_focus = false;
  bool m_has_focus = false;
  bool m_opened = false;
  bool m_dirty_aknowledged = false;

public:
  node_editor_window(const std::shared_ptr<node_editor_widget>& editor_widget)
    : editor(editor_widget)
  {
    m_window_title = "dead node";
    if (editor)
    {
      auto node = editor->node();
      if (node)
      {
        std::ostringstream ss;
        ss << "node:" << node->name();
        if (node->is_cnode())
          ss << " idx:" << node->idx();
        ss << "##node_editor_window_" << (uint64_t)node.get();
        m_window_title = ss.str();
      }
    }
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

    auto node = editor ? editor->node() : nullptr;
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

    if (ImGui::Begin(m_window_title.c_str(), &m_opened))
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
      ImGui::Text("Unsaved changes in %s, would you like to save ?", m_window_title.c_str());

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






