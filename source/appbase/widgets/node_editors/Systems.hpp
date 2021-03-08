#pragma once
#include <inttypes.h>
#include <sstream>
#include <spdlog/spdlog.h>
#include <imgui/imgui.h>

#include <appbase/extras/cpp_imgui.hpp>
#include <appbase/extras/imgui_stdlib.h>
#include <cpinternals/utils.hpp>
#include <cpinternals/ctypes.hpp>
#include "node_editor.hpp"
#include "hexedit.hpp"

#include "cpinternals/csav/nodes.hpp"

// to be used with CScriptObjProperty struct
struct CProperty_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CProperty& prop)
  {
    return prop.imgui_widget("Property", true);
  }
};

// to be used with CScriptObject struct
struct CObject_widget
{
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
  static std::string object_name_getter(const CObjectSPtr& item)
  {
    return std::string(item->ctypename().strv());
  };

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CSystem& sys, int* selected_object)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    auto& objects = sys.objects();

    if (objects.size() == 1)
    {
      ImGui::BeginChild("Object", ImVec2(-FLT_MIN, 0));
      auto& obj = objects[0];
      ImGui::PushItemWidth(300.f);
      auto ctype = obj->ctypename();
      ImGui::Text("object type: %s", ctype.c_str());
      ImGui::PopItemWidth();
      modified |= obj->imgui_widget(ctype.c_str(), true);
      ImGui::EndChild();
    }
    else
    {
      static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
        | ImGuiTableFlags_Resizable;

      ImVec2 size = ImVec2(-FLT_MIN, ImGui::GetContentRegionAvail().y);
      if (ImGui::BeginTable("##props", 2, tbl_flags, size))
      {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("objects", ImGuiTableColumnFlags_WidthFixed, 230.f);
        ImGui::TableSetupColumn("selected object", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        //ImGui::PushItemWidth(150.f);
        ImGui::BeginChild("Objects", ImVec2(-FLT_MIN, 0));
        modified |= ImGui::ErasableListBox("##CSystem::objects", selected_object, &object_name_getter, objects, true, true);
        ImGui::EndChild();
        //ImGui::PopItemWidth();

        ImGui::TableNextColumn();

        ImGui::BeginChild("Selected Object");

        int object_idx = *selected_object;
        if (object_idx >= 0 && object_idx < objects.size())
        {
          auto& obj = objects[object_idx];
          ImGui::PushItemWidth(300.f);
          auto ctype = obj->ctypename();
          ImGui::Text("object type: %s", ctype.c_str());
          ImGui::PopItemWidth();
          modified |= obj->imgui_widget(ctype.c_str(), true);
        }
        else
          ImGui::Text("no selected object");

        ImGui::EndChild();

        ImGui::EndTable();
      }
    }

    return modified;
  }
};


// to be used with CSystem struct
struct CPSData_widget
{
  static bool trailing_name_string_getter(void* data, int idx, const char** out)
  {
    const auto& tn = *(std::vector<CName>*)data;
    if (idx < 0 || idx >= tn.size())
      return false;
    *out = tn[idx].name().c_str();
    return true;
  }

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(cp::csav::CPSData& psdata, int* selected_object)
  {
    bool modified = false;

    if (psdata.m_vehicleGarageComponentPS)
    {
      ImGui::BeginChild("TopList");
      modified |= psdata.m_vehicleGarageComponentPS->imgui_widget("aa", true);
      ImGui::EndChild();
    }

    //ImGui::Text("PSData");
    modified |= CSystem_widget::draw(psdata.system(), selected_object);

    static int selected_dummy = -1;
    ImGui::ListBox("trailing names", &selected_dummy, &trailing_name_string_getter, (void*)&psdata.trailing_names, (int)psdata.trailing_names.size());

    return modified;
  }
};


class System_editor
  : public node_editor_widget
{
  cp::csav::CGenericSystem m_data;

public:
  System_editor(const std::shared_ptr<const cp::csav::node_t>& node, const cp::csav::version& version)
    : node_editor_widget(node, version)
  {
    reload();
  }

  ~System_editor() override {}

public:
  bool commit_impl() override
  {
    std::shared_ptr<const cp::csav::node_t> rebuilt;
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

  bool draw_impl(const ImVec2& size) override
  {
    scoped_imgui_id _sii(this);

    //ImGui::Text("System");

    return CSystem_widget::draw(m_data.system(), &selected_obj);
  }
};


class PSData_editor
  : public node_editor_widget
{
  cp::csav::CPSData m_data;

public:
  PSData_editor(const std::shared_ptr<const cp::csav::node_t>& node, const cp::csav::version& version)
    : node_editor_widget(node, version)
  {
    reload();
  }

  ~PSData_editor() override {}

public:
  bool commit_impl() override
  {
    std::shared_ptr<const cp::csav::node_t> rebuilt;
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

  bool draw_impl(const ImVec2& size) override
  {
    return CPSData_widget::draw(m_data, &selected_obj);;
  }
};

