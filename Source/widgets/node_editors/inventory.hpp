#pragma once
#include "node_editor.hpp"

#include "cpinternals/common.hpp"
#include "cpinternals/ctypes.hpp"
#include "cpinternals/csav/nodes/CInventory.hpp"
#include "itemData.hpp"


// to be used with CInventory struct
struct CInventory_widget
{
  //static CItemData copied_item = ;

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CInventory& inv, CStats* stats=nullptr)
  {
    if (!inv.has_valid_data)
      ImGui::Text("has invalid data");

    ImGui::BeginChild("##inventory_editor", ImVec2(0, 0), false, ImGuiWindowFlags_NoSavedSettings);
    //scoped_imgui_id _sii("##inventory_editor");
    bool modified = false;

    for (auto inv_it = inv.m_subinvs.begin(); inv_it != inv.m_subinvs.end(); ++inv_it)
    {
      ImGuiWindow* window = ImGui::GetCurrentWindow();
      ImGuiID row_id = window->GetID(&*inv_it);

      auto& subinv = *inv_it;


      std::string inv_label = "V's bag";
      switch (subinv.uid)
      {
        case 1: break;
        case 0x00000000000F4240: inv_label = "Car Stash"; break;
        case 0x38E8D0C9F9A087AE: inv_label = "Nomade Stash"; break;
        case 0x6E48C594562422DE: inv_label = "Judy's Stash"; break;
        case 0x7901DE03D136A5AF: inv_label = "V's Wardrobe"; break;
        case 0xEDAD8C9B086A615E: inv_label = "River's Stash"; break;
        default:
          inv_label = fmt::format("inventory_{:016X}", subinv.uid);break;
      }

      if (ImGui::TreeNodeBehavior(row_id, ImGuiTreeNodeFlags_Framed, inv_label.c_str()))
      {
        if (ImGui::Button("Sort (alpha)", ImVec2(0, 30)))
        {
          subinv.items.sort([](const CItemData& a, const CItemData& b){
            return a.name() < b.name();
          });
        }
        ImGui::SameLine();
        if (ImGui::Button("Add dummy item (alcohol6)", ImVec2(0, 30)))
        {
          // todo: move that on the data side
          CItemData item_data;
          item_data.iid.nameid.as_u64 = 0x1859EA0850; // Alcohol6
          item_data.iid.uk.uk4 = 2;
          item_data.uk1_012 = 0x213ACD;
          item_data.quantity = 1; // quantity
          subinv.items.insert(subinv.items.begin(), 1, item_data);
          modified = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Unflag all Quest items (makes them normal items)", ImVec2(0, 30)))
        {
          for (auto& item : subinv.items)
            item.flags &= 0xF8; // 3 bits
          modified = true;
        }

        static auto name_fn = [](const CItemData& item) { return item.iid.shortname(); };
        modified |= imgui_list_tree_widget(subinv.items, name_fn,
         [stats](CItemData& itemData) { return CItemData_widget::draw(itemData, stats); },
         0, true, false);

        ImGui::TreePop();
      }
    }
    ImGui::EndChild();

    return modified;
  }
};


