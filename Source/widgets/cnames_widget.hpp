#pragma once
#include <inttypes.h>
#include <AppLib/IApp.hpp>
#include <imgui_extras/imgui_better_combo.hpp>
#include <cpinternals/cpnames.hpp>

struct TweakDBID_widget
{
  struct ItemGetterData
  {
    std::string cur_name;
    const std::vector<std::string>& namelist;
  };

  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(TweakDBID& x, const char* label, TweakDBIDCategory cat = TweakDBIDCategory::All, bool advanced_edit=true)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    auto& namelist = TweakDBIDResolver::get().sorted_names(cat);

    // tricky ;)
    int item_current = 0;

    ItemGetterData data {x.name(), namelist};
    ImGui::BetterCombo(label, &item_current, &ItemGetter, (void*)&data, (int)namelist.size()+1);

    if (item_current != 0)
    {
      x = TweakDBID(namelist[item_current-1]);
      modified = true;
    }

    if (advanced_edit)
    {
      bool namehash_opened = ImGui::TreeNode("> edit tdbid by value");

      {
        scoped_imgui_style_color _stc(ImGuiCol_Text, ImColor::HSV(0.f, 1.f, 0.7f, 1.f).Value);
        ImGui::SameLine();
        ImGui::Text("(TweakDBID's DB is not complete yet !)"); // you can enter its hash manually too meanwhile
      }

      if (namehash_opened)
      {
        modified |= ImGui::InputScalar("crc32(name) hex",  ImGuiDataType_U32, &x.crc, NULL, NULL, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
        modified |= ImGui::InputScalar("length(name) hex", ImGuiDataType_U8,  &x.slen,  NULL, NULL, "%02X");
        modified |= ImGui::InputScalar("raw u64 hex",  ImGuiDataType_U64, &x.as_u64,  NULL, NULL, "%016llX", ImGuiInputTextFlags_CharsHexadecimal);
        //ImGui::Text("resolved name: %s", x.name().c_str());
        ImGui::TreePop();
      }
    }

    return modified;
  }

  static inline bool ItemGetter(void* data, int n, const char** out_str)
  { 
    auto& dataref = *(ItemGetterData*)data;
    if (n == 0)
      *out_str = dataref.cur_name.c_str();
    else
    {
      auto& s = dataref.namelist[n-1];
      auto cs = s.c_str();

      if (s.rfind("Items.", 0) == 0)
        *out_str = cs + 6;
      else if (s.rfind("AttachmentSlots.", 0) == 0)
        *out_str = cs + 16;
      else if (s.rfind("Vehicle.", 0) == 0)
        *out_str = cs + 8;
      else
        *out_str = cs;
    }
    return true;
  }
};


struct CName_widget
{
  // returns true if content has been edited
  [[nodiscard]] static inline bool draw(CName& x, const char* label)
  {
    scoped_imgui_id _sii(&x);
    bool modified = false;

    auto& namelist = CNameResolver::get().sorted_names();

    // tricky ;)
    int item_current = 0;

    const auto& curname = x.name();
    ImGui::BetterCombo(label, &item_current, &ItemGetter, (void*)curname.c_str(), (int)namelist.size()+1);

    if (item_current != 0)
    {
      x = CName(namelist[item_current-1]);
      modified = true;
    }

    bool namehash_opened = ImGui::TreeNode("> cname hash");

    {
      scoped_imgui_style_color _stc(ImGuiCol_Text, ImColor::HSV(0.f, 1.f, 0.7f, 1.f).Value);
      ImGui::SameLine();
      ImGui::Text("(CName's DB is not complete yet !)"); // you can enter its hash manually too meanwhile
    }

    if (namehash_opened)
    {
      modified |= ImGui::InputScalar("raw u64 hex",  ImGuiDataType_U64, &x.as_u64,  NULL, NULL, "%016llX", ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::Text("resolved name: %s", x.name().c_str());
      ImGui::TreePop();
    }

    return modified;
  }

  static inline bool ItemGetter(void* data, int n, const char** out_str)
  { 
    auto& namelist = CNameResolver::get().sorted_names();
    if (n == 0)
      *out_str = (const char*)data;
    else
      *out_str = namelist[n-1].c_str();
    return true;
  }
};

