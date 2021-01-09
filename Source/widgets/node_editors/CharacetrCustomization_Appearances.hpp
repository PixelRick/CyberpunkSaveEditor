#pragma once
#include "inttypes.h"
#include <sstream>
#include "fmt/format.h"
#include "utils.hpp"
#include "node_editor.hpp"
#include "cpinternals/cpnames.hpp"
#include "csav/cnodes/CCharacterCustomization.hpp"
#include "hexedit.hpp"
#include "imgui_extras/imgui_stdlib.h"


struct cetr_uk_thing5_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(cetr_uk_thing5& x)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    modified |= ImGui::InputText("uk_string_1##cetr5", &x.uk0);
    modified |= ImGui::InputText("uk_string_2##cetr5", &x.uk1);
    modified |= ImGui::InputText("uk_string_3##cetr5", &x.uk2);

    return modified;
  }
};


struct cetr_uk_thing4_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(cetr_uk_thing4& x)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    modified |= ImGui::InputText("uk_string_1##cetr4", &x.uk0);
    modified |= ImGui::InputText("uk_string_2##cetr4", &x.uk1);

    modified |= ImGui::InputScalar("uk_u32_1(hex)##cetr4", ImGuiDataType_U32, &x.uk2, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("uk_u32_2(hex)##cetr4", ImGuiDataType_U32, &x.uk3, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

    return modified;
  }
};

struct cetr_uk_thing3_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(cetr_uk_thing3& x)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    modified |= CName_widget::draw(x.cn, "some cname##cetr3");

    modified |= ImGui::InputText("property value##cetr3", &x.uk0);
    modified |= ImGui::InputText("property type##cetr3", &x.uk1);

    modified |= ImGui::InputScalar("uk_u32_1(hex)##cetr3", ImGuiDataType_U32, &x.uk2, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("uk_u32_2(hex)##cetr3", ImGuiDataType_U32, &x.uk3, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

    return modified;
  }
};

struct cetr_uk_thing2_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(cetr_uk_thing2& x)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    modified |= ImGui::InputText("name##cetr2", &x.uks);

    if (ImGui::TreeNode("some array #1##cetr2"))
    {
      static auto name_fn = [](const cetr_uk_thing3& y) { return y.cn.name(); };
      modified |= imgui_list_tree_widget(x.vuk3, name_fn, &cetr_uk_thing3_widget::draw, 0, false, false);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("some array #2##cetr2"))
    {
      static auto name_fn = [](const cetr_uk_thing4& y) { return y.uk0; };
      modified |= imgui_list_tree_widget(x.vuk4, name_fn, &cetr_uk_thing4_widget::draw, 0, false, false);
      ImGui::TreePop();
    }

    return modified;
  }
};

struct cetr_uk_thing1_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(cetr_uk_thing1& x)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    bool modified = false;

    static auto name_fn = [](const cetr_uk_thing2& y) { return y.uks; };
    modified |= imgui_list_tree_widget(x.vuk2, name_fn, &cetr_uk_thing2_widget::draw);

    return modified;
  }
};

// to be used with CSystem struct
struct CCharacterCustomization_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CCharacterCustomization& x)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    ImGui::BeginChild("##chter_editor", ImVec2(0, 0), false, ImGuiWindowFlags_NoSavedSettings);

    bool modified = false;

    modified |= ImGui::InputScalar("unknown #0 (08X)##cetr", ImGuiDataType_U32, &x.uk0, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("unknown #1 (08X)##cetr", ImGuiDataType_U32, &x.uk1, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("unknown #2 (02X)##cetr", ImGuiDataType_U8,  &x.uk2, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("unknown #3 (02X)##cetr", ImGuiDataType_U8,  &x.uk3, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);

    if (ImGui::TreeNode("struct #1##cetr"))
    {
      modified |= cetr_uk_thing1_widget::draw(x.ukt0);
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("struct #2##cetr"))
    {
      modified |= cetr_uk_thing1_widget::draw(x.ukt1);
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("struct #3##cetr"))
    {
      modified |= cetr_uk_thing1_widget::draw(x.ukt2);
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("array #1##cetr"))
    {
      static auto name_fn = [](const cetr_uk_thing5& y) { return std::string("unnamed"); };
      modified |= imgui_list_tree_widget(x.ukt5, name_fn, &cetr_uk_thing5_widget::draw, 0, false, false);
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("array #2##cetr"))
    {
      static auto name_fn = [](const std::string& y) { return y; };
      static auto edit_fn = [](std::string& y) { return ImGui::InputText("string", &y); };
      modified |= imgui_list_tree_widget(x.uk6s, name_fn, edit_fn, 0, false, false);
      ImGui::TreePop();
    }

    ImGui::EndChild();

    return modified;
  }
};


