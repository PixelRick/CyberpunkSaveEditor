#pragma once
#include "node_editor.hpp"
#include "cserialization/cpnames.hpp"
#include "cserialization/cnodes/inventory.hpp"

class itemData_editor
{
  
};

class inventory_editor
  : public node_editor
{
  // attrs
  inventory inv;

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
    return success;
  }

protected:
  void draw_impl(const ImVec2& size) override
  {
    auto& cpn = cpnames::get();

    ImGui::BeginChild("##inventory_editor",
      ImVec2(size.x > 0 ? size.x : 800, size.y > 0 ? size.y : 800));

    auto& emgr = node_editors_mgr::get();

    for (size_t j = 0; j < inv.m_subinvs.size(); ++j)
    {
      auto& subinv = inv.m_subinvs[j];
      ImGui::PushID((int)j);
      std::stringstream ss;
      ss << "inventory_" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << subinv.uid;
      if (ImGui::CollapsingHeader(ss.str().c_str()))
      {
        if (ImGui::Button("dupe first item row (don't worry about its name not changing during edit)") && subinv.items.size() > 0)
        {
          // todo: move that on the data side
          auto& first_item = subinv.items.front();
          auto& first_buf = first_item.item_node->data();
          auto nnode = node_t::create_shared(0, "itemData");
          nnode->nonconst().assign_data(first_buf.begin(), first_buf.end());

          subinv.items.insert(subinv.items.begin(), 1, inventory::item_entry_t{ first_item.id, nnode });
        }

        for (size_t i = 0; i < subinv.items.size(); ++i)
        {
          ImGui::PushID((int)i);
          if (ImGui::TreeNode(cpn.get_name(subinv.items[i].id).c_str()))
          {
            auto e = emgr.get_editor(subinv.items[i].item_node);
            e->draw_widget();
            ImGui::TreePop();
          }
          ImGui::PopID();
        }
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
  }
};

