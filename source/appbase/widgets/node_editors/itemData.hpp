#pragma once
#include <inttypes.h>


#include <redx/csav/nodes/inventory.h>

#include <appbase/IApp.hpp>
#include <appbase/widgets/list_widget.hpp>
#include <appbase/widgets/redx.hpp>
#include <appbase/widgets/node_editors/node_editor.hpp>

struct uk_thing_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(redx::csav::uk_thing& x)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    modified |= ImGui::InputScalar("u32 hash (or 2)##uk4",   ImGuiDataType_U32, &x.uk4, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputScalar("unknown u8  field##uk1", ImGuiDataType_U8,  &x.uk1,  NULL, NULL, "%u");
    modified |= ImGui::InputScalar("unknown u16 field##uk2", ImGuiDataType_U16, &x.uk2,  NULL, NULL, "%u");
    modified |= ImGui::InputScalar("unknown u8  field##uk5", ImGuiDataType_U8,  &x.uk5,  NULL, NULL, "%u");

    return modified;
  }
};

struct CItemID_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(redx::csav::CItemID& x, bool is_mod)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    if (ImGui::TreeNode("item_id_node", "item_id: %s", x.shortname().c_str()))
    {
      modified |= TweakDBID_widget::draw(x.nameid, "item name", TweakDBID_category::Item);

      //unsigned kind = x.uk.kind();
      //switch (kind) {
      //  case 0: ImGui::Text("resolved kind: special item"); break;
      //  case 1: ImGui::Text("resolved kind: simple item"); break;
      //  case 2: ImGui::Text("resolved kind: mod/modable item"); break;
      //  default: ImGui::Text("resolved kind: invalid"); break;
      //}

      if (ImGui::TreeNode("rngSeed (stats uid)"))
      {
        modified |= uk_thing_widget::draw(x.uk);
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }

    return modified;
  }
};

struct CUk0ID_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(redx::csav::CUk0ID& x)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    modified |= TweakDBID_widget::draw(x.nameid, "uk3.name", TweakDBID_category::Item);
    modified |= ImGui::InputScalar("uk3.uk0 (u32 hex)##uk4",   ImGuiDataType_U32, &x.uk0, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    //modified |= ImGui::InputScalar("field u32 (hex)##uk5",   ImGuiDataType_U32, &item.uk5, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    modified |= ImGui::InputFloat("uk3.weird_float ##uk5", &x.weird_float, NULL, NULL, "%.4e");

    return modified;
  }
};

struct CItemMod_widget
{
  // returns true if content has been edited
  static inline bool draw_no_stats(redx::csav::CItemMod& item)
  {
    return draw(item, nullptr);
  }

  [[nodiscard]] static inline bool draw(redx::csav::CItemMod& item, redx::csav::CStats* stats)
  {
    scoped_imgui_id _sii(&item);
    bool modified = false;


    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("itemData", 2, tbl_flags))
    {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("slot/mod data", ImGuiTableColumnFlags_WidthFixed, 350.f);
      ImGui::TableSetupColumn("attachments", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();

      modified |= CItemID_widget::draw(item.iid, true);

      //unsigned kind = item.iid.uk.kind();
      //switch (kind) {
      //  case 0: ImGui::Text("special kind"); break;
      //  case 1: ImGui::Text("simple kind"); break;
      //  default: break;
      //}

      ImGui::InputText("skin/variant name", item.cn0, sizeof(item.cn0));

      modified |= ImGui::InputScalar("field u32 (hex)##uk2",   ImGuiDataType_U32, &item.uk2, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);

      modified |= CUk0ID_widget::draw(item.uk3);

      ImGui::TableNextColumn();

      modified |= TweakDBID_widget::draw(item.tdbid1, "attachment slot name", TweakDBID_category::Attachment);

      ImGui::Text("slots/modifiers:");

      static auto name_fn = [](const redx::csav::CItemMod& mod) { return fmt::format("mod: {}", mod.iid.shortname().strv()); };
      modified |= imgui_list_tree_widget(item.subs, name_fn, &CItemMod_widget::draw_no_stats, 0, true);

      ImGui::EndTable();
    }

    return modified;
  }
};


// to be used with CItemData struct
struct CItemData_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(redx::csav::CItemData& item, redx::csav::CStats* stats=nullptr)
  {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID("itemData");

    if (!item.has_valid_data) {
        ImGui::Text("Item couldn't be loaded for edition.");
        return false;
    }

    bool modified = false;

    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable
      | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable("itemData", 2, tbl_flags))
    {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("item data", ImGuiTableColumnFlags_WidthFixed, 450.f);
      ImGui::TableSetupColumn("mods data", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();


      ImGui::TableNextRow();
      ImGui::TableNextColumn();

      modified |= CItemID_widget::draw(item.iid, false);

      int flgs = item.flags;
      modified |= ImGui::CheckboxFlags("Quest Item", &flgs, 1);
      modified |= ImGui::CheckboxFlags("Special Item", &flgs, 2);
      modified |= ImGui::CheckboxFlags("Special Item (PS4)", &flgs, 4);
      item.flags = (uint8_t)flgs;


      unsigned kind = item.iid.uk.kind();
      

      if (kind != 2)
      {
        modified |= ImGui::InputScalar("Quantity##uk2", ImGuiDataType_U32, &item.quantity, NULL, NULL);
      }

      modified |= ImGui::InputScalar("flags (hex)##uk0", ImGuiDataType_U8 , &item.flags, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      modified |= ImGui::InputScalar("unknown u32 (hex)##uk1", ImGuiDataType_U32, &item.uk1_012, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);


      if (kind != 1 && ImGui::Button("make item legendary"))
      {
        auto modifiers = stats == nullptr
          ? nullptr
          : dynamic_cast<CDynArrayProperty*>(stats->get_modifiers_prop(item.iid.uk.uk4));

        if (modifiers)
        {
          // https://github.com/Deweh/CyberCAT-SimpleGUI/blob/master/CP2077SaveEditor/ItemDetails.cs
          try
          {
            bool found_qstat = false;
            for (auto& o : *modifiers)
            {
              auto stat = dynamic_cast<CHandleProperty*>(o.get())->obj();
              if (stat->ctypename() != gname("gameConstantStatModifierData_Deprecated"))
                continue;
              if (stat->get_prop_cast<CEnumProperty>("statType"_gndef)->value_name() == "Quality"_gndef)
              {
                stat->get_prop_cast<CFloatProperty>("value"_gndef)->set_value(4.0f);
                stat->get_prop_cast<CEnumProperty>("modifierType"_gndef)->set_value_by_name("Additive"_gndef);
                found_qstat = true;
                break;
              }
            }
            if (!found_qstat)
            {
              auto stat = stats->add_constant_stats(modifiers);
              if (stat)
              {
                stat->get_prop_cast<CFloatProperty>("value"_gndef)->set_value(4.0f);
                stat->get_prop_cast<CEnumProperty>("modifierType"_gndef)->set_value_by_name("Additive"_gndef);
                stat->get_prop_cast<CEnumProperty>("statType"_gndef)->set_value_by_name("Quality"_gndef);
              }
            }
          }
          catch (std::exception& e)
          {
            MessageBoxA(0, e.what(), "error", 0);
          }
        }
        else
        {
          item.uk3.nameid.as_u64 = 0x14951C01A5;
        }
      }


      ImGui::Text("-----Stats-----");
      if (stats != nullptr)
      {
        auto modifiers = dynamic_cast<CDynArrayProperty*>(stats->get_modifiers_prop(item.iid.uk.uk4));
        if (modifiers)
        {
          if (ImGui::Button("new constant"))
            stats->add_constant_stats(modifiers);
          ImGui::SameLine();
          if (ImGui::Button("new curve"))
            stats->add_curve_stats(modifiers);
          ImGui::SameLine();
          if (ImGui::Button("new combined"))
            stats->add_combined_stats(modifiers);

          modified |= modifiers->imgui_widget("item_stats", true);
        }
      }

      ImGui::TableNextColumn();

      if (kind != 1)
      {
        modified |= CUk0ID_widget::draw(item.uk3);

        if (ImGui::SmallButton("clear mods"))
        {
          modified = true;
          item.root2 = redx::csav::CItemMod();
        }

        ImGui::Text("MOD ROOT:");
        modified |= CItemMod_widget::draw_no_stats(item.root2);
      }
      else
      {
        ImGui::Text("not modable");
      }

      ImGui::EndTable();
    }

    return modified;
  }
};

