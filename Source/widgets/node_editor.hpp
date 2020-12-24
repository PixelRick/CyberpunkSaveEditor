#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <sstream>

#include "AppLib/IApp.h"
#include "cserialization/node.hpp"

/*
struct node_dataview
{
std::shared_ptr<node_t> node;

virtual void commit() = 0;
virtual void reload() = 0;
};
*/

#define NODE_EDITOR__DEFAULT_EDITOR_NAME "<default_editor>"

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

public:
  static inline std::shared_ptr<node_editor>
  create(const std::shared_ptr<const node_t>& node)
  {
    auto it = s_factory_map.find(node->name());
    if (it == s_factory_map.end())
      it = s_factory_map.find(NODE_EDITOR__DEFAULT_EDITOR_NAME);
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

public:
  std::shared_ptr<const node_t> node() const { return node_view::node(); }

  void draw_window(bool* p_open = nullptr)
  {
    if (ImGui::Begin(m_window_name.c_str(), &m_window_opened))
    {
      draw_widget();
    }
    ImGui::End();
    if (p_open)
      *p_open = m_window_opened;
  }

  void draw_widget()
  {
    draw_impl();

    if (ImGui::BeginPopupModal("Error##node_editor", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::NewLine();

      ImGui::Text("%s", s_error.c_str());

      ImGui::Separator();
      if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        ImGui::CloseCurrentPopup();

      ImGui::EndPopup();
    }

    if (ImGui::Button("commit changes##node_editor"))
      commit();
    ImGui::SameLine();
    if (ImGui::Button("discard changes##node_editor"))
      reload();
  }

  static void error(std::string_view msg)
  {
    s_error = msg;
    ImGui::OpenPopup("Error##node_editor");
  }

  virtual void commit() = 0;
  virtual void reload() = 0;

protected:
  virtual void draw_impl() = 0;
};

