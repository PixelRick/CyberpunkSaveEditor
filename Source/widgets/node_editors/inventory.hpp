#pragma once
#include "node_editor.hpp"
#include "cserialization/cpnames.hpp"
#include "cserialization/cnodes/inventory.hpp"
#include "itemData.hpp"

class inventory_editor
  : public node_editor_widget
{
  inventory inv;

public:
  inventory_editor(const std::shared_ptr<const node_t>& node)
    : node_editor_widget(node)
  {
    reload();
  }

  ~inventory_editor() override {}

public:
  bool commit_impl() override
  {
    auto rebuilt = inv.to_node();
    auto curnode = ncnode();

    if (!rebuilt)
      return false;

    curnode->assign_children(rebuilt->children());
    curnode->assign_data(rebuilt->data());
    return true;
  }

  bool reload_impl() override
  {
    bool success = inv.from_node(node());
    return success;
  }

protected:
  void draw_impl(const ImVec2& size) override
  {
    auto& cpn = cpnames::get();

    scoped_imgui_id _sii("##inventory_editor");


    for (auto inv_it = inv.m_subinvs.begin(); inv_it != inv.m_subinvs.end(); ++inv_it)
    {
      ImGuiWindow* window = ImGui::GetCurrentWindow();
      ImGuiID row_id = window->GetID(&*inv_it);

      auto& subinv = *inv_it;

      std::stringstream ss;
      ss << "inventory_" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << subinv.uid;
      if (ImGui::TreeNodeBehavior(row_id, ImGuiTreeNodeFlags_Framed, ss.str().c_str()))
      {
        if (ImGui::Button("dupe first row") && subinv.items.size() > 0)
        {
          // todo: move that on the data side
          auto& first_item = subinv.items.front();
          auto new_node = first_item.to_node()->deepcopy();
          itemData item_data;
          item_data.from_node(new_node);
          subinv.items.insert(subinv.items.begin(), 1, item_data);
        }

        for (auto it = subinv.items.begin(); it != subinv.items.end();)
        {
          scoped_imgui_id _sii(&*it);
          bool torem = false;
          itemData_widget::draw(*it, &torem, 0);
          if (torem)
            it = subinv.items.erase(it);
          else
            ++it;
        }

        ImGui::TreePop();
      }
    }
  }
};

