#pragma once
#include <inttypes.h>
#include <AppLib/IApp.hpp>
#include <cserialization/cnodes/CItemData.hpp>
#include <widgets/list_widget.hpp>
#include <widgets/cnames_widget.hpp>
#include "node_editor.hpp"

struct uk_thing_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(uk_thing& x)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    modified |= ImGui::InputScalar("u32 hash (or 2)##uk4",   ImGuiDataType_U32, &x.uk4, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("unknown u8  field##uk1", ImGuiDataType_U8,  &x.uk1,  NULL, NULL, "%u");
    modified |= ImGui::InputScalar("unknown u16 field##uk2", ImGuiDataType_U16, &x.uk2,  NULL, NULL, "%u");

    return modified;
  }
};

struct CItemID_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CItemID& x, bool is_mod)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    if (ImGui::TreeNode("item_id", "item_id: %s", x.shortname().c_str()))
    {
      modified |= TweakDBID_widget::draw(x.nameid, "item name", TweakDBIDCategory::Item);

      unsigned kind = x.uk.kind();
      switch (kind) {
        case 0: ImGui::Text("resolved kind: special item"); break;
        case 1: ImGui::Text("resolved kind: simple item"); break;
        case 2: ImGui::Text("resolved kind: mod/modable item"); break;
        default: ImGui::Text("resolved kind: invalid"); break;
      }

      if (ImGui::TreeNode("unique identifier"))
      {
        modified |= uk_thing_widget::draw(x.uk);
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }

    return modified;
  }
};

struct CItemMod_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CItemMod& item)
  {
    scoped_imgui_id _sii(&item);
    bool modified = false;

    modified |= CItemID_widget::draw(item.iid, true);

    ImGui::Separator();

    unsigned kind = item.iid.uk.kind();
    switch (kind) {
      case 0: ImGui::Text("special kind"); break;
      case 1: ImGui::Text("simple kind"); break;
      default: break;
    }

    ImGui::InputText("unknown string", item.uk0, sizeof(item.uk0));

    modified |= TweakDBID_widget::draw(item.uk1, "attachment slot name", TweakDBIDCategory::Attachment);

    ImGui::Text("--------slots/modifiers---------");

    static auto name_fn = [](const CItemMod& mod) { return fmt::format("mod: {}", mod.iid.shortname()); };
    modified |= imgui_list_tree_widget(item.subs, name_fn, &CItemMod_widget::draw, 0, true);

    ImGui::Text("--------------------------------");

    modified |= ImGui::InputScalar("field u32 (hex)##uk2",   ImGuiDataType_U32, &item.uk2, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

    modified |= TweakDBID_widget::draw(item.uk3, "uk3 name");

    modified |= ImGui::InputScalar("field u32 (hex)##uk4",   ImGuiDataType_U32, &item.uk4, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("field u32 (hex)##uk5",   ImGuiDataType_U32, &item.uk5, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

    return modified;
  }
};


// to be used with CItemData struct
struct CItemData_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CItemData& item)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID("itemData");

    bool modified = false;

    modified |= CItemID_widget::draw(item.iid, false);

    modified |= ImGui::InputScalar("field u8  (hex) (1 for quest items)##uk0",   ImGuiDataType_U8 , &item.uk0_012, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("field u32 (hex)##uk1",   ImGuiDataType_U32, &item.uk1_012, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

    unsigned kind = item.iid.uk.kind();

    if (kind != 2)
    {
      ImGui::Text("------special/simple------ (often quantity value)");
      modified |= ImGui::InputScalar("field u32 (hex)##uk2", ImGuiDataType_U32, &item.uk2_01, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    }

    if (kind != 1)
    {
      ImGui::Text("-------special/mods-------");

      modified |= TweakDBID_widget::draw(item.uk3_02, "uk3 name");
      modified |= ImGui::InputScalar("field u32 (hex)##uk4", ImGuiDataType_U32, &item.uk4_02, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
      modified |= ImGui::InputScalar("field u32 (hex)##uk5", ImGuiDataType_U32, &item.uk5_02, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

      if (ImGui::SmallButton("clear mods"))
      {
        modified = true;
        item.root2 = CItemMod();
      }

      if (ImGui::TreeNode("mods_root##CItemData", "MODS"))
      {
        modified |= CItemMod_widget::draw(item.root2);
        ImGui::TreePop();
      }
    }

    return modified;
  }
};

// to be used with CItemData node
class itemData_editor
  : public node_editor_widget
{
  CItemData item;

public:
  itemData_editor(const std::shared_ptr<const node_t>& node, const csav_version& version)
    : node_editor_widget(node, version)
  {
    reload();
  }

  ~itemData_editor() {}

public:
  bool commit_impl() override
  {
    auto rebuilt = item.to_node(version());
    auto curnode = ncnode();

    if (!rebuilt)
      return false;

    curnode->assign_children(rebuilt->children());
    curnode->assign_data(rebuilt->data());
    return true;
  }

  bool reload_impl() override
  {
    bool success = item.from_node(node(), version());
    return success;
  }

protected:
  bool draw_impl(const ImVec2& size) override
  {
    scoped_imgui_id _sii(this);
    return CItemData_widget::draw(item);
  }
};