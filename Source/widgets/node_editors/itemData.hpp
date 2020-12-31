#pragma once
#include "inttypes.h"
#include "node_editor.hpp"
#include "cserialization/cpnames.hpp"
#include "cserialization/cnodes/CItemData.hpp"

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

struct namehash_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(namehash& x, const char* label)
  {
    scoped_imgui_id _sii(label);
    bool modified = false;

    // tricky ;)
    int item_current = 0;
    ImGui::Combo(label, &item_current, &ItemGetter, (void*)x.name().c_str(), (int)cpnames::get().namelist().size()+1);
    if (item_current != 0)
    {
      x = namehash(cpnames::get().namelist()[item_current-1]);
      modified = true;
    }

    bool namehash_opened = ImGui::TreeNode("> namehash");

    {
      scoped_imgui_style_color _stc(ImGuiCol_Text, ImColor::HSV(0.f, 1.f, 0.7f, 1.f).Value);
      ImGui::SameLine();
      ImGui::Text("(item names db is not complete yet !)"); // you can enter its hash manually too meanwhile
    }

    if (namehash_opened)
    {
      modified |= ImGui::InputScalar("crc32(name)",  ImGuiDataType_U32, &x.crc, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
      modified |= ImGui::InputScalar("length(name)", ImGuiDataType_U8,  &x.slen,  NULL, NULL, "%u");
      modified |= ImGui::InputScalar("raw u64 hex",  ImGuiDataType_U64, &x.as_u64,  NULL, NULL, "%016llX", ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::Text("resolved name: %s", x.name().c_str());
      ImGui::TreePop();
    }
  
    return modified;
  }

  static inline bool ItemGetter(void* data, int n, const char** out_str)
  { 
    if (n == 0)
      *out_str = (const char*)data;
    else
      *out_str = cpnames::get().namelist()[n-1].c_str();
    return true;
  }
};

struct item_id_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CItemID& x)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    if (ImGui::TreeNode("item_id", "item_id: %s", x.shortname().c_str()))
    {
      modified |= namehash_widget::draw(x.nameid, "item name");

      unsigned kind = x.uk.kind();
      switch (kind) {
        case 0: ImGui::Text("resolved kind: special item"); break;
        case 1: ImGui::Text("resolved kind: simple item"); break;
        case 2: ImGui::Text("resolved kind: modable item"); break;
        default: ImGui::Text("resolved kind: invalid"); break;
      }

      if (ImGui::TreeNode("kind struct"))
      {
        modified |= uk_thing_widget::draw(x.uk);
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }

    return modified;
  }
};

struct item_mod_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CItemMod& item, bool* p_remove = nullptr)
  {
    scoped_imgui_id _sii(&item);
    bool modified = false;

    bool treenode = ImGui::TreeNode("item_mod", "item_mod: %s", item.iid.shortname().c_str());
    
    if (p_remove)
    {
      ImGui::SameLine();
      *p_remove = ImGui::Button("remove", ImVec2(70, 15));
    }

    if (treenode)
    {
      modified |= item_id_widget::draw(item.iid);

      unsigned kind = item.iid.uk.kind();
      switch (kind) {
        case 0: ImGui::Text("special kind"); break;
        case 1: ImGui::Text("simple kind"); break;
        default: break;
      }

      ImGui::InputText("unknown string", item.uk0, sizeof(item.uk0));

      modified |= namehash_widget::draw(item.uk1, "uk1 name");

      ImGui::Text("--------mods-tree---------");

      if (item.subs.empty())
        ImGui::Text(" empty");

      for (auto it = item.subs.begin(); it < item.subs.end();)
      {
        bool torem = false;
        modified |= item_mod_widget::draw(*it, &torem);
        if (torem) {
          it = item.subs.erase(it);
          modified = true;
        }
        else
          ++it;
      }

      ImGui::Text("--------------------------");

      modified |= ImGui::InputScalar("field u32 (hex)##uk2",   ImGuiDataType_U32, &item.uk2, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

      modified |= namehash_widget::draw(item.uk3, "uk3 name");

      modified |= ImGui::InputScalar("field u32 (hex)##uk4",   ImGuiDataType_U32, &item.uk4, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
      modified |= ImGui::InputScalar("field u32 (hex)##uk5",   ImGuiDataType_U32, &item.uk5, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

      //e->draw_widget();
      ImGui::TreePop();
    }

    return modified;
  }
};



// to be used with CItemData struct
struct itemData_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CItemData& item, bool* p_remove = nullptr)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID("itemData");

    bool modified = false;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed;
    if (p_remove)
      flags |= ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton;

    bool treenode = ImGui::TreeNodeBehavior(id, flags, item.iid.shortname().c_str());
    if (p_remove)
    {
      // copied from CollapsingHeader impl
      ImGuiContext& g = *GImGui;
      ImGuiLastItemDataBackup last_item_backup;
      float button_size = g.FontSize;
      float button_x = ImMax(window->DC.LastItemRect.Min.x, window->DC.LastItemRect.Max.x - g.Style.FramePadding.x * 2.0f - button_size);
      float button_y = window->DC.LastItemRect.Min.y;
      ImGuiID close_button_id = ImGui::GetIDWithSeed("#CLOSE", NULL, id);
      if (ImGui::CloseButton(close_button_id, ImVec2(button_x, button_y)))
        *p_remove = true;
      last_item_backup.Restore();

      //ImGui::SameLine();
      //*p_remove = ImGui::Button("remove", ImVec2(70, 15));
    }
    if (treenode)
    {
      modified |= item_id_widget::draw(item.iid);

      modified |= ImGui::InputScalar("field u8  (hex) (1 for quest items)##uk0",   ImGuiDataType_U8 , &item.uk0_012, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      modified |= ImGui::InputScalar("field u32 (hex)##uk1",   ImGuiDataType_U32, &item.uk1_012, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

      unsigned kind = item.iid.uk.kind();

      if (kind != 2)
      {
        ImGui::Text("------special/simple------ (often quantity value)");
        modified |= ImGui::InputScalar("field u32 (hex)##uk2", ImGuiDataType_U32, &item.uk2_01, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      }

      if (kind != 1)
      {
        ImGui::Text("-------special/mods-------");

        modified |= namehash_widget::draw(item.uk3_02, "uk3 name");
        modified |= ImGui::InputScalar("field u32 (hex)##uk4", ImGuiDataType_U32, &item.uk4_02, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
        modified |= ImGui::InputScalar("field u32 (hex)##uk5", ImGuiDataType_U32, &item.uk5_02, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

        bool torem = false;
        modified |= item_mod_widget::draw(item.root2, &torem);
        if (torem) {
          item.root2 = CItemMod();
          modified = true;
        }
      }

      ImGui::TreePop();
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
    scoped_imgui_id _sii(this);

    bool changes = itemData_widget::draw(item, 0);
    if (changes)
      m_has_unsaved_changes = true;
  }
};