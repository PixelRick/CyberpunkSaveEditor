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

  template <typename T>
  struct VectorListBox
  {
    using ItemGetterFn = std::function<std::string(const T&)>;

    VectorListBox(ItemGetterFn&& items_getter, const std::vector<T>& items)
      : fn(std::forward<ItemGetterFn>(items_getter))
      , items(items)
    {
    }

    [[nodiscard]] bool draw(std::string_view label, int* current_item, int height_items = -1)
    {
      return ListBox(label.data(), current_item, &items_getter, (void*)this, (int)items.size(), height_items);
    }

  protected:
    static inline bool items_getter(void* data, int idx, const char** out_pcstr)
    {
      const VectorListBox& vdata = *(const VectorListBox*)data;

      if (idx < 0 || idx > vdata.items.size())
        return false;

      static std::string tmp;
      tmp = vdata.fn(vdata.items[(size_t)idx]);
      *out_pcstr = tmp.c_str();
      return true;
    }

    ItemGetterFn fn;
    const std::vector<T>& items;
  };


  template <typename T>
  bool ListBox(std::string_view label, int* current_item, typename VectorListBox<T>::ItemGetterFn&& items_getter, const std::vector<T>& items, int height_items = -1)
  {
    VectorListBox<T> vlb(std::forward<typename VectorListBox<T>::ItemGetterFn>(items_getter), items);
    return vlb.draw(label.data(), current_item, height_items);
  }

  // returns true if data has been modified
  template <typename T, typename ItemNameGetFnT,
    std::enable_if_t<
      std::is_convertible_v<ItemNameGetFnT&&, std::function<std::string(const T&)>>
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
        std::string item_name = std::forward<ItemNameGetFnT>(item_name_getter)(items[i]);
        if (item_name.empty())
          item_name = "*Unknown item*";

        std::string row_label = item_name;
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

