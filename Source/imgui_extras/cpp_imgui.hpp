#pragma once
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include <fmt/format.h>
#include <imgui.h>

namespace ImGui
{
  bool ListBox(std::string_view label, int* current_item, const std::vector<std::string>& items, int height_items = -1);

  // returns true if data has been modified
  template <typename T, typename ItemNameGetFnT,
    std::enable_if_t<
      std::is_convertible_v<ItemNameGetFnT&&, std::function<std::string_view(const T&)>>
    , int> = 0
  >
  bool ErasableListBox(
    const char*         label,
    int*                current_item,
    ItemNameGetFnT&&    item_name_getter,
    std::vector<T>&     items,
    bool                erase_disabled = false,
    bool                numbered_labels = false,
    int                 height_in_items = -1)
  {
    //if (!ListBoxHeader(label, (int)items.size(), height_in_items))
    //  return false;

    const ImGuiID list_item_id = GetID(label);

    int torem_idx = -1;
    ImGuiContext& g = *ImGui::GetCurrentContext();
    bool value_changed = false;
    ImGuiListClipper clipper;
    clipper.Begin((int)items.size(), GetTextLineHeightWithSpacing());

    while (clipper.Step())
    {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
      {
        const bool item_selected = (i == *current_item);
        std::string_view item_name = std::forward<ItemNameGetFnT>(item_name_getter)(items[i]);
        if (item_name.empty())
          item_name = "*Unknown item*";

        std::string row_label = std::string(item_name);
        if (numbered_labels)
          row_label = fmt::format("{:>3d} {}", i, row_label);

        PushID(i);
        if (Selectable(row_label.c_str(), item_selected))
        {
          *current_item = i;
        }
        if (!erase_disabled && ImGui::BeginPopupContextItem("item context menu"))
        {
          if (ImGui::Selectable("delete"))
            torem_idx = i;
          ImGui::EndPopup();
        }

        if (item_selected)
          SetItemDefaultFocus();
        PopID();
      }
    }
    //ListBoxFooter();

    if (torem_idx != -1)
    {
      items.erase(items.begin() + torem_idx);
      value_changed = true;
      if (*current_item == torem_idx)
        *current_item = -1;
    }

    //if (value_changed)
    //  MarkItemEdited(list_item_id);

    return value_changed;
  }

}

