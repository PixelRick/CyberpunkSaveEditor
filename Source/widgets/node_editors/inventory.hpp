#pragma once
#include "node_editor.hpp"
#include "cserialization/cpnames.hpp"
#include "cserialization/cnodes/inventory.hpp"




struct itemData2_widget
{
  static inline void draw(data_2& item, bool* p_remove = nullptr, bool* p_modified = nullptr)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(&item);

    bool treenode = ImGui::TreeNode(&item, item.iid.name().c_str());

    if (0 && p_remove)
    {
      ImGui::SameLine();
      *p_remove = ImGui::Button("remove", ImVec2(70, 15));
    }
    if (treenode)
    {
      unsigned kind = item.iid.uk.kind();
      std::stringstream ss;
      ss << "itemData2 ikind:" << (int)kind << "\n";

      auto& cpn = cpnames::get();

      /*
      item_id iid;
      std::string uk0;
      namehash uk1;
      std::vector<data_2> subs;
      uint32_t uk2;
      namehash uk3;
      uint32_t uk4;
      uint32_t uk5;
      */

      ss << "uk0:" << item.uk0 << "\n";
      ss << "uk1:" << cpn.get_name(item.uk1) << "\n";
      ss << "uk2:" << item.uk2 << "\n";
      ss << "uk3:" << cpn.get_name(item.uk3) << "\n";
      ss << "uk4:" << item.uk4 << "\n";
      ss << "uk5:" << item.uk5 << "\n";

      ImGui::Text(ss.str().c_str());

      for (auto& sub : item.subs)
        itemData2_widget::draw(sub, p_remove, p_modified);

      //e->draw_widget();
      ImGui::TreePop();
    }
  }
};



// to be used with itemData struct
struct itemData_widget
{
  static inline void draw(itemData& item, bool* p_remove = nullptr, bool* p_modified = nullptr)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(&item);

    bool treenode = ImGui::TreeNode(&item, item.iid.name().c_str());

    if (p_remove)
    {
      ImGui::SameLine();
      *p_remove = ImGui::Button("remove", ImVec2(70, 15));
    }
    if (treenode)
    {
      if (ImGui::Button("click here to open the hex editor"))
        auto e = node_editor_windows_mgr::get().open_window(item.raw, true);

      unsigned kind = item.iid.uk.kind();
      std::stringstream ss;
      ss << "itemData ikind:" << (int)kind;
      ImGui::Text(ss.str().c_str());

      auto& cpn = cpnames::get();

      if (kind == 2)
      {
        std::stringstream ss;
        ss << "uk2_0:" << (size_t)item.uk2_0 << "\n";
        ss << "uk2_1:" << item.uk2_1 << "\n";
        ss << "uk2_2:" << cpn.get_name(item.uk2_2) << "\n";
        ss << "uk2_3:" << item.uk2_3 << "\n";
        ss << "uk2_4:" << item.uk2_4 << "\n";
        ss << "root2:\n";
        ImGui::Text(ss.str().c_str());

        itemData2_widget::draw(item.root2, 0, p_modified);
      }

      //e->draw_widget();
      ImGui::TreePop();
    }
  }
};

// to be used with itemData node
class itemData_editor
  : public node_editor_widget
{
  itemData item;

public:
  itemData_editor(const std::shared_ptr<const node_t>& node)
    : node_editor_widget(node)
  {
    reload();
  }

  ~itemData_editor() {}

public:
  bool commit_impl() override
  {
    auto rebuilt = item.to_node();
    auto curnode = ncnode();

    if (!rebuilt)
      return false;

    curnode->assign_children(rebuilt->children());
    curnode->assign_data(rebuilt->data());
  }

  bool reload_impl() override
  {
    bool success = item.from_node(node());
    return success;
  }

protected:
  void draw_impl(const ImVec2& size) override
  {
    bool changes = false;
    itemData_widget::draw(item, 0, &changes);
    if (changes)
      m_has_unsaved_changes = true;
  }
};

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

    ImGui::BeginChild("##inventory_editor",
      ImVec2(size.x > 0 ? size.x : 800, size.y > 0 ? size.y : 600));

    for (size_t j = 0; j < inv.m_subinvs.size(); ++j)
    {
      auto& subinv = inv.m_subinvs[j];
      ImGui::PushID((int)j);
      std::stringstream ss;
      ss << "inventory_" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << subinv.uid;
      if (ImGui::CollapsingHeader(ss.str().c_str()))
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

        int torem_item = -1;
        for (size_t i = 0; i < subinv.items.size(); ++i)
        {
          ImGui::PushID((int)i);

          bool rem = false;
          auto& item_data = subinv.items[i];
          itemData_widget::draw(item_data, &rem);
          if (rem)
            torem_item = (int)i;

          ImGui::PopID();
        }
        if (torem_item >= 0)
          subinv.items.erase(subinv.items.begin() + torem_item);
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
  }
};

