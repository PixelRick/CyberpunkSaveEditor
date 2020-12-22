#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <map>

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

#define DEFAULT_EDITOR_NAME "<default_editor>"

class node_editor
  : protected node_view
{
  // inlined factory -start-

  using create_fn_t = std::shared_ptr<node_editor>(*)(const std::shared_ptr<const node_t>& node);
  using factory_map_t = std::map<std::string, create_fn_t>;

  static inline factory_map_t s_factory_map;

public:
  static inline std::shared_ptr<node_editor>
  factory_create(const std::shared_ptr<const node_t>& node)
  {
    auto it = s_factory_map.find(node->name());
    if (it == s_factory_map.end())
      it = s_factory_map.find(DEFAULT_EDITOR_NAME);
    if (it != s_factory_map.end())
      return it->second(node);
    return nullptr;
  }

  static inline void
  factory_register(const std::string& node_name, create_fn_t create_fn)
  {
    s_factory_map[node_name] = create_fn;
  }

  // inlined factory -end-

private:
  static inline std::string s_error;

protected:
  node_editor(const std::shared_ptr<const node_t>& node)
    : node_view(node) {}
  ~node_editor() override {};

public:
  void draw()
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

