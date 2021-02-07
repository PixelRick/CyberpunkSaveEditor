#pragma once
#include <appbase/IApp.hpp>
#include <list>


inline bool imgui_close_button(ImGuiID id, const ImVec2& pos, const float height)
{
  ImGuiContext& g = *GImGui;
  ImGuiWindow* window = g.CurrentWindow;

  const ImRect bb(pos, pos + ImVec2(height, height));
  if (!ImGui::ItemAdd(bb, id))
    return false;

  bool hovered, held;
  bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

  // Render
  ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
  ImVec2 center = bb.GetCenter();
  if (hovered)
    window->DrawList->AddCircleFilled(center, ImMax(2.0f, height * 0.5f), col, 12);

  float cross_extent = height * 0.5f * 0.7071f - 1.0f;
  ImU32 cross_col = ImGui::GetColorU32(ImGuiCol_Text);
  center -= ImVec2(0.5f, 0.5f);
  window->DrawList->AddLine(center + ImVec2(+cross_extent, +cross_extent), center + ImVec2(-cross_extent, -cross_extent), cross_col, 1.0f);
  window->DrawList->AddLine(center + ImVec2(+cross_extent, -cross_extent), center + ImVec2(-cross_extent, +cross_extent), cross_col, 1.0f);

  return pressed;
}

// returns true on modified
template <typename T, typename GetTNameStringFn, typename DrawTContentFn>
inline bool imgui_list_tree_widget(std::list<T>& l, GetTNameStringFn&& name_fn, DrawTContentFn&& draw_fn, ImGuiTreeNodeFlags flags = 0, bool erasable_items=true, bool default_insertable=true, uint32_t clipcnt = 0)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  bool modified = false;

  if (default_insertable && ImGui::SmallButton("add new entry"))
  {
    modified = true;
    l.push_front(T{}); // todo: CreateNewTFn parameter
  }

  if (l.empty())
  {
    ImGui::Text("empty list");
    return false;
  }

  if (erasable_items && (flags & ImGuiTreeNodeFlags_SpanAvailWidth))
    flags |= ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton;

  scoped_imgui_id _sii(&l);

  for (auto it = l.begin(); it != l.end();)
  {
    scoped_imgui_id _sii1(&*it);

    auto label = std::forward<GetTNameStringFn>(name_fn)(*it);
    auto id = window->GetID(&*it);
    bool expand = ImGui::TreeNodeBehavior(window->GetID(&*it), flags, label.c_str());

    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiLastItemDataBackup last_item_backup {};
    /*
    float height = window->DC.LastItemRect.GetHeight();
    float button_x = window->DC.LastItemRect.Max.x;
    if (flags & ImGuiTreeNodeFlags_SpanAvailWidth)
      button_x = ImMax(window->DC.LastItemRect.Min.x, window->DC.LastItemRect.Max.x - g.Style.FramePadding.x * 2.0f - height);
    float button_y = window->DC.LastItemRect.Min.y;
    ImGuiID close_button_id = ImGui::GetIDWithSeed("#CLOSE", NULL, id);
    bool removed = imgui_close_button(close_button_id, ImVec2(button_x, button_y), height);
    */
    bool removed = false;
    if (erasable_items)
    {
      //scoped_imgui_id _sii2("#smallremove");
      ImGui::SameLine();
      removed = ImGui::SmallButton("remove");
    }
    last_item_backup.Restore();

    if (expand)
    {
      modified |= std::forward<DrawTContentFn>(draw_fn)(*it);
      ImGui::TreePop();
    }

    if (removed)
    {
      it = l.erase(it);
      modified = true;
    }
    else
      ++it;
  }

  return modified;
}

