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
  std::map<uint64_t, std::shared_ptr<node_editor>> m_item_widgets;

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
        for (size_t i = 0; i < subinv.items.size(); ++i)
        {
          ImGui::PushID((int)i);
          if (ImGui::CollapsingHeader(cpn.get_name(subinv.items[i].id).c_str()))
          {
            
          }
          ImGui::PopID();
        }
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
  }
};

