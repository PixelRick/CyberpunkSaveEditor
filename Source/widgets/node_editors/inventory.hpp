#pragma once
#include "node_editor.hpp"
#include "cserialization/cpnames.hpp"
#include "cserialization/cnodes/inventory.hpp"

class inventory_editor
  : public node_editor
{
  // attrs
  inventory inv;
  std::vector<std::shared_ptr<node_editor>> item_editors;

public:
  inventory_editor(const std::shared_ptr<const node_t>& node)
    : node_editor(node)
  {
    reload();
  }

public:
  bool commit_impl() override
  {
    //inv.to_node();

    return false;

  }

  bool reload_impl() override
  {
    bool success = inv.from_node(node());
    item_editors.clear();
    //for (auto& e : inv.m_items)
    //  item_editors.push_back(node_editor::create(e.item));
    return success;
  }

protected:
  void draw_impl(const ImVec2& size) override
  {
    auto& cpn = cpnames::get();

    ImGui::BeginChild("##inventory_editor",
      ImVec2(size.x > 0 ? size.x : 800, size.y > 0 ? size.y : 800));

    for (size_t j = 0; j < inv.m_subinvs.size(); ++j)
    {
      auto& subinv = inv.m_subinvs[j];
      ImGui::PushID((int)j);
      if (ImGui::CollapsingHeader("sub-inventory"))
      {
        ImGui::Text("todo, %d %d %d", subinv.ukcnt0, subinv.ukcnt1, subinv.ukcnt2);
        for (size_t i = 0; i < subinv.items.size(); ++i)
        {
          ImGui::PushID((int)i);
          if (ImGui::CollapsingHeader(cpn.get_name(subinv.items[i].id).c_str())) {
            if (i < item_editors.size()) item_editors[i]->draw_widget();
          }
          ImGui::PopID();
        }
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
  }
};

