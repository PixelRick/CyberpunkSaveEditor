#pragma once
#include <inttypes.h>
#include <appbase/IApp.hpp>
#include <appbase/extras/imgui_better_combo.hpp>
#include "redx/csav/nodes/questSystem/FactsDB.hpp"
#include <appbase/widgets/redx.hpp>

namespace UI {

struct WidFactsTable
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(redx::csav::FactsTable& x, const char* label)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    scoped_imgui_id _sii(&x);
    bool modified = false;

    auto& facts = x.facts();

    static ImGuiTableFlags tbl_flags =
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float full_height = line_height * static_cast<float>(facts.size());

    ImGui::Text("Actions:");
    ImGui::SameLine();
    if (ImGui::Button("Prepend new fact"))
    {
      facts.emplace(facts.begin());
    }
    ImGui::SameLine();
    if (ImGui::Button("Sort facts lexicographically ascending"))
    {
      std::sort(facts.begin(), facts.end(), [](const redx::CFact& a, const redx::CFact& b) -> bool {
        return a.name().strv() < b.name().strv();
      });
    }


    int torem_idx = -1;
    ImVec2 size = ImVec2(-FLT_MIN, std::min(600.f, full_height));
    if (ImGui::BeginTable(label, 1, tbl_flags))
    {
      // TODO: use clipper
      int i = 0;
      for (auto& fact : facts)
      {
        scoped_imgui_id __sii(&fact);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (ImGui::Button("delete"))
        {
          torem_idx = i;
        }
        ImGui::SameLine();
        modified |= WidCFact::draw(fact);

        ++i;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Button("append new fact"))
      {
        facts.emplace_back();
      }

      if (torem_idx != -1)
      {
        facts.erase(facts.begin() + torem_idx);
        modified = true;
      }

      ImGui::EndTable();
    }

    return modified;
  }
};


struct WidFactsDB
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(redx::csav::FactsDB& x, const char* label)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
      return false;

    scoped_imgui_id _sii(&x);
    bool modified = false;

    static ImGuiTabBarFlags tab_bar_flags =
      ImGuiTabBarFlags_Reorderable |
      //ImGuiTabBarFlags_AutoSelectNewTabs |
      ImGuiTabBarFlags_FittingPolicyResizeDown;

    ImGui::Separator();

    if (ImGui::BeginTabBar(label, tab_bar_flags))
    {
      size_t i = 0;
      for (auto& table : x.tables())
      {
        auto tab_label = fmt::format("FactsTable#{}", i);
        if (ImGui::BeginTabItem(tab_label.c_str(), 0, ImGuiTabItemFlags_None))
        {
          ImGui::BeginChild("##FactsTableScroll", ImVec2(0, 0), false, ImGuiWindowFlags_NoSavedSettings);
          modified |= WidFactsTable::draw(table, tab_label.c_str());
          ImGui::EndChild();

          ImGui::EndTabItem();
        }
        ++i;
      }

      ImGui::EndTabBar();
    }

    return modified;
  }
};

} // namespace UI

