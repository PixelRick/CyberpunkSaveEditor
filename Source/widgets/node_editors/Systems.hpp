#pragma once
#include <inttypes.h>
#include <sstream>
#include <fmt/format.h>
#include <imgui.h>

#include <imgui_extras/cpp_imgui.hpp>
#include <imgui_extras/imgui_stdlib.h>
#include <utils.hpp>
#include <cpinternals/cpnames.hpp>
#include <cserialization/cnodes/CSystems.hpp>
#include "node_editor.hpp"
#include "hexedit.hpp"

// to be used with CScriptObjProperty struct
struct CProperty_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(const CPropertySPtr& prop)
  {
    return draw(*prop);
  }

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CProperty& prop)
  {
    return prop.imgui_widget("Property", true);
  }
};

// to be used with CScriptObject struct
struct CObject_widget
{
  static std::string_view prop_name_getter(const CPropertyField& field)
  {
    return field.name;
  };

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(const CObjectSPtr& pobj, int* selected_property)
  {
    return draw(*pobj, selected_property);
  }

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CObject& obj, int* selected_property)
  {
    return obj.imgui_widget("Property", true);
  }
};



// to be used with CSystem struct
struct CSystem_widget
{
  static std::string_view object_name_getter(const CSystemObject& item)
  {
    return item.name;
  };

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CSystem& sys, int* selected_object, int* selected_property)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    auto& objects = sys.objects();

    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

    ImVec2 size = ImVec2(-FLT_MIN, std::min(600.f, ImGui::GetContentRegionAvail().y));
    if (ImGui::BeginTable("##props", 2, tbl_flags, size))
    {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("objects", ImGuiTableColumnFlags_WidthFixed, 200.f);
      ImGui::TableSetupColumn("selected object", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();

      //ImGui::PushItemWidth(150.f);
      ImGui::BeginChild("Objects");
      modified |= ImGui::ErasableListBox("##CSystem::objects", selected_object, &object_name_getter, objects, true, true);
      ImGui::EndChild();
      //ImGui::PopItemWidth();

      ImGui::TableNextColumn();

      int object_idx = *selected_object;
      if (object_idx >= 0 && object_idx < objects.size())
      {
        auto& obj = objects[object_idx];
        ImGui::PushItemWidth(300.f);
        ImGui::Text("object type: %s", obj.name.c_str());
        ImGui::PopItemWidth();
        modified |= obj.obj->imgui_widget(obj.name.c_str(), true);
      }
      else
        ImGui::Text("no selected object");

      ImGui::EndTable();
    }

    return modified;
  }
};





class System_editor
  : public node_editor_widget
{
  CSystemGeneric m_data;

public:
  System_editor(const std::shared_ptr<const node_t>& node, const csav_version& version)
    : node_editor_widget(node, version)
  {
    reload();
  }

  ~System_editor() override {}

public:
  bool commit_impl() override
  {
    std::shared_ptr<const node_t> rebuilt;
    try
    {
      rebuilt = m_data.to_node(version());
    }
    catch (std::exception& e)
    {
      error(e.what());
    }

    auto curnode = ncnode();

    if (!rebuilt)
      return false;

    curnode->assign_children(rebuilt->children());
    curnode->assign_data(rebuilt->data());
    return true;
  }

  bool reload_impl() override
  {
    bool success = false;
    try
    {
      success = m_data.from_node(node(), version());
    }
    catch (std::exception& e)
    {
      error(e.what());
    }
    return success;
  }

protected:
  int selected_obj = -1;
  int selected_prop = -1;

  bool draw_impl(const ImVec2& size) override
  {
    scoped_imgui_id _sii(this);

    //ImGui::Text("System");

    return CSystem_widget::draw(m_data.system(), &selected_obj, &selected_prop);
  }
};


class PSData_editor
  : public node_editor_widget
{
  CPSData m_data;

public:
  PSData_editor(const std::shared_ptr<const node_t>& node, const csav_version& version)
    : node_editor_widget(node, version)
  {
    reload();
  }

  ~PSData_editor() override {}

public:
  bool commit_impl() override
  {
    std::shared_ptr<const node_t> rebuilt;
    try
    {
      rebuilt = m_data.to_node(version());
    }
    catch (std::exception& e)
    {
      error(e.what());
    }

    auto curnode = ncnode();

    if (!rebuilt)
      return false;

    curnode->assign_children(rebuilt->children());
    curnode->assign_data(rebuilt->data());
    return true;
  }

  bool reload_impl() override
  {
    bool success = false;
    try
    {
      success = m_data.from_node(node(), version());
    }
    catch (std::exception& e)
    {
      error(e.what());
    }
    return success;
  }

protected:
  int selected_obj = -1;
  int selected_prop = -1;
  int selected_dummy = -1;

  static bool trailing_name_string_getter(void* data, int idx, const char** out)
  {
    const auto& tn = *(std::vector<CName>*)data;
    if (idx < 0 || idx >= tn.size())
      return false;
    static std::string dummy_str;
    dummy_str = tn[idx].str();
    *out = dummy_str.c_str();
    return true;
  }

  bool draw_impl(const ImVec2& size) override
  {
    scoped_imgui_id _sii(this);

    //ImGui::Text("PSData");
    bool modified = CSystem_widget::draw(m_data.system(), &selected_obj, &selected_prop);

    ImGui::ListBox("trailing names", &selected_dummy, &trailing_name_string_getter, (void*)&m_data.trailing_names, (int)m_data.trailing_names.size());

    return modified;
  }
};

