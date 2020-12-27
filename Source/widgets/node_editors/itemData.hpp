#pragma once
#include "node_editor.hpp"
#include "cserialization/cpnames.hpp"
#include "cserialization/cnodes/itemData.hpp"

struct uk_thing_widget
{
  static inline void draw(uk_thing& x, bool* p_modified = nullptr)
  {
    scoped_imgui_id _sii(&x);

    ImGui::InputScalar("u32 hash (or 2)##uk4",   ImGuiDataType_U32, &x.uk4, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::InputScalar("unknown u8  field##uk1", ImGuiDataType_U8,  &x.uk1,  NULL, NULL, "%u");
    ImGui::InputScalar("unknown u16 field##uk2", ImGuiDataType_U16, &x.uk2,  NULL, NULL, "%u");
  }
};

struct namehash_widget
{
  static inline void draw(namehash& x, bool* p_modified = nullptr)
  {
    scoped_imgui_id _sii(&x);

    ImGui::Text("resolved name: %s", x.name().c_str());
    
    {
      scoped_imgui_text_color _stc(ImGuiCol_Text, ImColor::HSV(0.f, 1.f, 0.7f, 1.f).Value);
      ImGui::SameLine();
      ImGui::Text("(item names db is not complete yet !)"); // you can enter its hash manually too meanwhile
    }

    ImGui::InputScalar("crc32(name)",  ImGuiDataType_U32, &x.crc, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::InputScalar("length(name)", ImGuiDataType_U8,  &x.slen,  NULL, NULL, "%u");
    ImGui::InputScalar("raw u64 hex",  ImGuiDataType_U64, &x.as_u64,  NULL, NULL, "%016X", ImGuiInputTextFlags_CharsHexadecimal);
  }
};

struct item_id_widget
{
  static inline void draw(item_id& x, bool* p_modified = nullptr)
  {
    scoped_imgui_id _sii(&x);
    if (ImGui::TreeNode("item_id", "item_id: %s", x.shortname().c_str()))
    {
      if (ImGui::TreeNode("namehash"))
      {
        namehash_widget::draw(x.nameid, p_modified);
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("kind struct"))
      {
        uk_thing_widget::draw(x.uk, p_modified);
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }
  }
};

struct item_mod_widget
{
  static inline void draw(item_mod& item, bool* p_remove = nullptr, bool* p_modified = nullptr)
  {
    scoped_imgui_id _sii(&item);

    bool closable_group = true;

    bool treenode = ImGui::TreeNode("item_mod", "item_mod: %s", item.iid.shortname().c_str());
    
    if (p_remove)
    {
      ImGui::SameLine();
      *p_remove = ImGui::Button("remove", ImVec2(70, 15));
    }

    if (treenode)
    {
      item_id_widget::draw(item.iid);

      unsigned kind = item.iid.uk.kind();
      switch (kind) {
        case 0: ImGui::Text("special kind"); break;
        case 1: ImGui::Text("simple kind"); break;
        default: break;
      }

      ImGui::InputText("unknown string", item.uk0, sizeof(item.uk0));
      if (ImGui::TreeNode("uk1", "uk1: %s", item.uk1.shortname().c_str()))
      {
        namehash_widget::draw(item.uk1);
        ImGui::TreePop();
      }

      ImGui::Text("--------mods-tree---------");

      if (item.subs.empty())
        ImGui::Text(" empty");

      for (auto it = item.subs.begin(); it < item.subs.end();)
      {
        bool torem = false;
        item_mod_widget::draw(*it, &torem, p_modified);
        if (torem)
          it = item.subs.erase(it);
        else
          ++it;
      }

      ImGui::Text("--------------------------");

      ImGui::InputScalar("field u32 (hex)##uk2",   ImGuiDataType_U32, &item.uk2, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

      if (ImGui::TreeNode("uk3", "uk3: %s", item.uk3.shortname().c_str()))
      {
        namehash_widget::draw(item.uk3);
        ImGui::TreePop();
      }

      ImGui::InputScalar("field u32 (hex)##uk4",   ImGuiDataType_U32, &item.uk4, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::InputScalar("field u32 (hex)##uk5",   ImGuiDataType_U32, &item.uk5, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

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
    ImGuiID id = window->GetID("itemData");

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
      item_id_widget::draw(item.iid);

      unsigned kind = item.iid.uk.kind();
      switch (kind) {
        case 0: ImGui::Text("resolved kind: special item"); break;
        case 1: ImGui::Text("resolved kind: simple item"); break;
        case 2: ImGui::Text("resolved kind: modable item"); break;
        default: ImGui::Text("resolved kind: invalid"); break;
      }

      ImGui::InputScalar("field u8  (hex)##uk0",   ImGuiDataType_U8 , &item.uk0_012, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::InputScalar("field u32 (hex)##uk1",   ImGuiDataType_U32, &item.uk1_012, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

      if (kind != 2)
      {
        ImGui::Text("------special/simple------ (often quantity value)");
        ImGui::InputScalar("field u32 (hex)##uk2", ImGuiDataType_U32, &item.uk2_01, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      }

      if (kind != 1)
      {
        ImGui::Text("-------special/mods-------");
        if (ImGui::TreeNode("namehash##uk3"))
        {
          namehash_widget::draw(item.uk3_02, p_modified);
          ImGui::TreePop();
        }
        ImGui::InputScalar("field u32 (hex)##uk4", ImGuiDataType_U32, &item.uk4_02, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("field u32 (hex)##uk5", ImGuiDataType_U32, &item.uk5_02, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

        bool torem = false;
        item_mod_widget::draw(item.root2, &torem, p_modified);
        if (torem)
          item.root2 = item_mod();
      }

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
    scoped_imgui_id _sii(this);

    bool changes = false;
    itemData_widget::draw(item, 0, &changes);
    if (changes)
      m_has_unsaved_changes = true;
  }
};