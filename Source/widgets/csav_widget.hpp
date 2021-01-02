#pragma once

#include <stdint.h>
#include <thread>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>
#include <vector>
#include <memory>

#include "AppLib/IApp.hpp"

#include "utils.hpp"
#include "cserialization/csav.hpp"
#include "cpinternals/cpnames.hpp"
#include "hexeditor_windows_mgr.hpp"
#include "node_editors.hpp"

void ImGui::ShowDemoWindow(bool* p_open);

using loading_bar_job_fntype = bool (*)(float& progress);

class loading_bar_job_widget
{
  std::thread thread;
  bool finished = false;
  float progress = 0.f;

public:
  bool failed = false;

  bool is_running() const { return thread.joinable(); }

  template <class Fn, std::enable_if_t<std::is_same_v<std::invoke_result_t<Fn, float&>, bool>, int> = 0> 
  bool start(Fn&& fn)
  {
    if (is_running())
      return false;
    failed = false;
    progress = 0.f;
    thread = std::thread([this, fn]() {
      failed = !fn(progress);
      finished = true;
    });
    return true;
  }

  void update()
  {
    if (finished) {
      finished = false;
      thread.join();
    }
  }

  void draw()
  {
    if (is_running()) {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(1) << (progress * 100) << "%";
      ImGui::ProgressBar(progress, ImVec2(ImGui::GetContentRegionAvailWidth(), 0.f), ss.str().c_str());
    }
  }
};

static inline bool s_use_ps4_weird_format = false;
static inline bool s_dump_decompressed_data = false;

class csav_collapsable_header
{
protected:
  ImGui::FileBrowser save_dialog;
  static inline loading_bar_job_widget save_job; // only one save job

  std::shared_ptr<csav> m_csav;
  std::shared_ptr<AppImage> m_img;
  std::string m_pretty_name;

  std::vector<std::shared_ptr<node_editor_widget>> m_collapsible_editors;

  bool m_closed = false; // can be destroyed
  bool m_closing = false; // close button clicked

  //std::vector<ScanEntryWidget> scan_entries;
  //using scan_entry_it = decltype(scan_entries)::iterator;
  std::array<char, 24 * 3 + 1> search_needle = {};
  std::array<char, 24     + 1> search_mask = {};

public:
  csav_collapsable_header(const std::shared_ptr<csav>& csav, const std::shared_ptr<AppImage>& img, std::string_view name = "")
    : save_dialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir)
    , m_csav(csav), m_img(img)
  {
    if (name.empty())
    {
      if (csav)
        m_pretty_name = (csav->filepath.parent_path().filename().string() / csav->filepath.filename()).string();
      else
        m_pretty_name = "dead";
    }
    else
      m_pretty_name = name;

    save_dialog.SetTitle("Saving savefile");
    save_dialog.SetTypeFilters({ ".dat" });

    if (m_csav)
    {
      add_collapsible_editor<inventory_editor>("inventory");
    }
  }

  ~csav_collapsable_header()
  {
    // fail-safe, not the best
    while (save_job.is_running())
      Sleep(1);
  }

  template <typename EditorType>
  void add_collapsible_editor(std::string_view node_name)
  {
    auto node = m_csav->search_node(node_name);
    if (node)
      m_collapsible_editors.push_back(std::make_shared<EditorType>(node, m_csav->ver));
  }

protected:
  void do_save()
  {
    std::weak_ptr<csav> weak_csav = m_csav;
    save_job.start([weak_csav](float& progress) -> bool {
      auto csav = weak_csav.lock();
      return csav->save_with_progress(csav->filepath, progress, s_dump_decompressed_data, s_use_ps4_weird_format);
    });
    ImGui::OpenPopup("Saving..##SAVE"); // should be in parent class
  }

public:
  void close()
  {
    m_closing = true;
  }

  bool is_closed() const
  {
    return m_closed;
  }

  void update()
  {
    save_job.update();
  }

  const std::string& pretty_name() 
  {
    return m_pretty_name;
  }

  static inline std::shared_ptr<const node_t> appearance_src;
  static inline csav_version appearance_version {};

  void draw()
  {
    scoped_imgui_id sii {this};
    ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

    std::string label = fmt::format("{} (csav {})",
      m_csav->filepath.u8string(), m_csav->ver.string());

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, style.ItemSpacing.y));

    //-------------------------------------------------------------------------------

    ImGui::ButtonEx("", ImVec2(20, 60), ImGuiButtonFlags_Disabled);

    //-------------------------------------------------------------------------------

    ImGui::SameLine();
    float post_arrow_x = ImGui::GetCursorPosX();

    if (m_img)
      m_img->draw(ImVec2(107, 60));
    else
    {
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK);
      ImGui::ButtonEx("nopic##screenshot", ImVec2(107, 60), ImGuiButtonFlags_Disabled);
      ImGui::PopStyleColor();
    }

    //-------------------------------------------------------------------------------

    ImGui::SameLine();
    ImGui::ButtonEx("", ImVec2(20, 60), ImGuiButtonFlags_Disabled);

    //-------------------------------------------------------------------------------

    bool should_do_save = false;

    {
      scoped_imgui_button_hue _sibh(0.2f);
      ImGui::SameLine();
      bool save_clicked = ImGui::ButtonEx("SAVE##SAVE", ImVec2(120, 60));
      if (!save_job.is_running() && !m_closed && save_clicked)
      {
        bool has_unsaved_changes = false;
        for (auto& ce : m_collapsible_editors)
        {
          if (ce->has_changes())
          {
            has_unsaved_changes = true;
            break;
          }
        }
        if (has_unsaved_changes)
        {
          ImGui::OpenPopup("Unsaved changes##csav_editor");
        }
        else if (save_clicked)
        {
          should_do_save = true;
        }
      }
    }

    bool is_being_warned = false;

    // Always center this window when appearing
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Unsaved changes##csav_editor", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
      is_being_warned = true;
      ImGui::Text("Unsaved changes in inventory!!\nWould you like to save them first ?");

      float button_width = ImGui::GetContentRegionAvail().x * 0.25f;
      ImGui::Separator();
      if (ImGui::Button("NO", ImVec2(button_width, 0)))
      {
        should_do_save = true;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("YES", ImVec2(button_width, 0)))
      {
        for (auto& ce : m_collapsible_editors)
        {
          if (ce->has_changes())
            ce->commit();
        }
        should_do_save = true;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("CANCEL", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
      {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (should_do_save)
      do_save();

    if (!save_job.is_running())
    {
      if (m_closing)
        m_closed = true;
    }

    bool pushed_stylecol = false;
    if (save_job.failed) {
      ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, (ImVec4)ImColor::HSV(0.f, 1.f, 0.6f, 0.35f));
      pushed_stylecol = true;
    }

    // Always center this window when appearing
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Saving..##SAVE", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text("path: %s", m_csav->filepath.string().c_str());

      if (save_job.is_running())
      {
        save_job.draw();
      }
      else 
      {
        if (save_job.failed)
          ImGui::Text("error, couldn't save savefile");
        else
          ImGui::Text("success!");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
          ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (pushed_stylecol)
      ImGui::PopStyleColor();

    // todo: save as.. (dialog)

    //-------------------------------------------------------------------------------

    ImGui::SameLine();
    if (ImGui::ButtonEx("COPY SKIN##SAVE", ImVec2(100, 60)))
    {
      appearance_src = m_csav->search_node("CharacetrCustomization_Appearances");
      appearance_version = m_csav->ver;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Copy skin in app clipboard.");

    //-------------------------------------------------------------------------------

    ImGui::SameLine();
    if (ImGui::ButtonEx("PASTE SKIN##SAVE", ImVec2(100, 60)))
    {
      auto appearance_node = m_csav->search_node("CharacetrCustomization_Appearances");
      if (appearance_src && appearance_src != appearance_node)
      {
        if (appearance_version != m_csav->ver)
        {
          ImGui::OpenPopup("Error##TRANSFER");
        }
        else
        {
          auto& src_buf = appearance_src->data();
          appearance_node->nonconst().assign_data(src_buf.begin(), src_buf.end());
        }
      }
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Paste skin from app clipboard.\nDon't forget to SAVE afterwards!");

    // Always center this window when appearing
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Error##TRANSFER", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text(" saves' version mismatch ");
      ImGui::Text(" -> update your saves ingame first ");
      ImGui::Separator();
      if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        ImGui::CloseCurrentPopup();

      ImGui::EndPopup();
    }

    //-------------------------------------------------------------------------------

    ImGui::SameLine();
    ImGui::ButtonEx(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 60), ImGuiButtonFlags_Disabled);

    //-------------------------------------------------------------------------------

    ImGui::PopStyleVar();

    const float indent = 12.f;

    //const ImGuiID id = ImGui::GetCurrentWindow()->GetID((void*)this);
    //ImGui::BeginChild(id);
    ImGui::BeginChild(ImGui::GetID("csav_frame"), ImVec2(0, 0), false);
    draw_content();
    ImGui::EndChild();
    //ImGui::EndChild();
  }

  void draw_content()
  {
    for (auto& ce : m_collapsible_editors)
    {
      scoped_imgui_id _sii((void*)&ce);

      if (ce && ImGui::CollapsingHeader(ce->node_name().c_str(), ImGuiTreeNodeFlags_None))
      {
        ImGui::Indent(5.f);
        ce->draw_widget();
        ImGui::Unindent(5.f);
      }
    }

    ImGui::Separator();
    ImGui::Text("for devs:");

    if (ImGui::CollapsingHeader("Original Node Descriptors", ImGuiTreeNodeFlags_None))
    {
      ImGui::Indent(5.f);

      static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
        | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

      static float u32row_width = ImGui::CalcTextSize("0xFFFFFFFF ").x;

      ImVec2 size = ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 30);
      if (ImGui::BeginTable("##nodedescs_table", 6, flags, size))
      {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("idx", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("next idx", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("child idx", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("offset", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableSetupColumn("size", ImGuiTableColumnFlags_WidthFixed, u32row_width);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)m_csav->node_descs.size());
        while (clipper.Step())
        {
          for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
          {
            auto& desc = m_csav->node_descs[row];
            scoped_imgui_id sii{&desc};
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("0x%X", row);
            ImGui::TableNextColumn(); ImGui::Text("%s", desc.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.next_idx);
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.child_idx);
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.data_offset);
            ImGui::TableNextColumn(); ImGui::Text("0x%X", desc.data_size);
          }
        }
        ImGui::EndTable();
      }

      ImGui::Unindent(5.f);
    }

    if (ImGui::CollapsingHeader("Node Tree", ImGuiTreeNodeFlags_None))
    {
      ImGui::Indent(5.f);

      if (m_csav->root_node)
        for (const auto& n : m_csav->root_node->children())
          draw_tree_node(n);

      ImGui::Unindent(5.f);
    }

    if (ImGui::CollapsingHeader("Search Tools", ImGuiTreeNodeFlags_None))
    {
      ImGui::Indent(5.f);

      int line_width = (int)ImGui::GetContentRegionAvail().x;
      float slider_width = (float)std::max(line_width - 300, 100);

      static char search_text[256];
      const bool text_search = ImGui::Button("search text", ImVec2(150, 0)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputText("input text", search_text, 256);
      const bool crc32_search = ImGui::Button("search crc32", ImVec2(150, 0)); ImGui::SameLine();
      TweakDBID nhash(search_text);
      std::stringstream ss;
      for (size_t i = 0; i < sizeof(TweakDBID); ++i)
        ss << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << (uint32_t)*((uint8_t*)&nhash.as_u64 + i);
      ImGui::Text("namehash:%s (crc32 on 4 bytes, strlen on 1 byte, 3 zeroes) crc32=0x%08X",
        ss.str().c_str(), nhash.crc);

      if (text_search) {
        search_pattern_in_nodes(search_text, "");
      }
      if (crc32_search) {
        search_pattern_in_nodes(std::string((char*)&nhash.crc, (char*)&nhash.crc + 4), "");
      }

      static uint32_t u32_v;
      const bool u32_search = ImGui::Button("search u32", ImVec2(150, 0)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputScalar("input u32 (dec)", ImGuiDataType_U32, &u32_v);
      ImGui::InvisibleButton("search u32##next", ImVec2(150, 1)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputScalar("input u32 (hex)", ImGuiDataType_U32, &u32_v, 0, 0, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
      if (u32_search) {
        search_pattern_in_nodes(std::string((char*)&u32_v, (char*)&u32_v + 4), "");
      }

      static float f32_v;
      const bool f32_search = ImGui::Button("search float", ImVec2(150, 0)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputScalar("input float", ImGuiDataType_Float, &f32_v, 0, 0);
      if (f32_search) {
        search_pattern_in_nodes(std::string((char*)&f32_v, (char*)&f32_v + 4), "");
      }

      static double f64_v;
      const bool f64_search = ImGui::Button("search double", ImVec2(150, 0)); ImGui::SameLine();
      ImGui::PushItemWidth(slider_width);
      ImGui::InputScalar("input double", ImGuiDataType_Double, &f64_v);
      if (f64_search) {
        search_pattern_in_nodes(std::string((char*)&f64_v, (char*)&f64_v + 8), "");
      }

      ImGui::Separator();
      if (ImGui::Button("search bytes from editor clipboard (context/copy)"))
      {
        auto& cp = node_hexeditor::clipboard();
        std::string needle(cp.begin(), cp.end());
        if (needle.size())
          search_pattern_in_nodes(needle, "");
      }

      ImGui::Separator();

      // bool pat_search = ImGui::Button("search pattern", ImVec2(150, 0)); ImGui::SameLine();
      // ImGui::PushItemWidth(slider_width);
      // ImGui::InputText("pattern bytes", search_needle.data(), (int)search_needle.size(), ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AlwaysInsertMode);
      // ImGui::InvisibleButton("search pattern##inv", ImVec2(150, 1)); ImGui::SameLine();
      // ImGui::PushItemWidth(slider_width);
      // ImGui::InputTextEx("pattern mask", "x?xxx??x", search_mask.data(), (int)search_mask.size(), ImVec2(0, 0), ImGuiInputTextFlags_AlwaysInsertMode);
      
      // todo, parse hex string

      ImGui::Separator();

      // results

      static std::shared_ptr<node_hexeditor> nh;

      static float u32row_width = ImGui::CalcTextSize("0xFFFFFFFF ").x;

      static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
        | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
      ImVec2 size = ImVec2(500, ImGui::GetTextLineHeightWithSpacing() * 28);
      if (ImGui::BeginTable("##searchres_table2", 1, flags, size))
      {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("matches", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)search_result.size());
        while (clipper.Step())
        {
          for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
          {
            auto& match = search_result[row];
            scoped_imgui_id sii{&match};
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            
            static char matchname[512];
            ImFormatString(matchname, 512, "offset 0x%08X in node %4d - %s", match.offset, match.n->idx(), match.n->name().c_str());

            if (ImGui::Selectable(matchname, selected_result == row))
            {
              selected_result = row;
              if (!nh || nh->node() != match.n)
                nh = std::make_shared<node_hexeditor>(match.n);
              nh->select(match.offset, match.size);
            }
          }
        }
        ImGui::EndTable();
      }


      if (nh) {
        bool opened = true;
        ImGui::SameLine();
        nh->draw_widget();
        if (!opened)
          nh.reset();
      }

      ImGui::Unindent(5.f);
    }
  }

protected:
  void draw_tree_node(const std::shared_ptr<const node_t>& node)
  {
    if (!node)
    {
      ImGui::Text("null");
      return;
    }

    auto& emgr = hexeditor_windows_mgr::get();
    auto editor = emgr.find_window(node);

    const bool selected = editor && editor->is_opened();
    const bool focused = editor && editor->has_focus();

    ImGuiTreeNodeFlags node_flags = selected ? ImGuiTreeNodeFlags_Selected : 0;
    if (!node->has_children())
      node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    if (focused)
    {
      auto& focus_col = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);
      ImGui::PushStyleColor(ImGuiCol_HeaderActive, focus_col);
      ImGui::PushStyleColor(ImGuiCol_Header, focus_col);
    }

    bool opened =
      (node->is_blob() or node->is_root())
      ? ImGui::TreeNodeEx((void*)node.get(), node_flags, "%s", node->name().c_str())
      : ImGui::TreeNodeEx((void*)node.get(), node_flags, "%s (%u)", node->name().c_str(), node->idx());

    if (focused)
    {
      ImGui::PopStyleColor(2);
    }

    if (ImGui::IsItemClicked())
    {
      if (ImGui::IsMouseDoubleClicked(0))
        editor = emgr.open_window(node);

      if (editor)
        editor->take_focus();
    }

    if (opened && node->has_children())
    {
      for (auto& child : node->children())
        draw_tree_node(child);

      ImGui::TreePop();
    }
  }

  // hex search.. 

  struct search_match {
    std::shared_ptr<const node_t> n;
    size_t offset;
    size_t size;
  };
  std::vector<search_match> search_result;
  size_t selected_result = (size_t)-1;

  std::vector<search_match>& search_pattern_in_nodes(const std::string& needle, const std::string& mask)
  {
    selected_result = (size_t)-1;
    // history draft
    /*
    std::stringstream ss;
    ss << "bytes:";
    ss << std::hex << std::uppercase;
    for (auto c : needle)
      ss << std::setfill('0') << std::setw(2) << c << " ";
    if (mask.size())
      ss << "mask:" << mask;
    auto it = searches.emplace(ss.str(), std::vector<search_match>());
    */
    search_result.clear();
    if (m_csav and !std::all_of(needle.begin(), needle.end(), [](char i) { return i==0; })) // searching for zeroes is just.. the way to die
      search_pattern_in_nodes_rec(search_result, m_csav->root_node, needle, mask);
    return search_result;
  }

  void search_pattern_in_nodes_rec(std::vector<search_match>& matches, const std::shared_ptr<const node_t> node, const std::string& needle, const std::string& mask)
  {
    auto& haystack = node->data();
    std::vector<uintptr_t> match_offsets;
    if (mask.size())
      match_offsets = sse2_strstr_masked((uint8_t*)haystack.data(), haystack.size(), (uint8_t*)needle.data(), needle.size(), mask.data(), mask.size());
    else
      match_offsets = sse2_strstr((uint8_t*)haystack.data(), haystack.size(), (uint8_t*)needle.data(), needle.size());
    for (auto off : match_offsets)
      matches.emplace_back(search_match{node, off, needle.size()});
    for (auto& c : node->children())
      search_pattern_in_nodes_rec(matches, c, needle, mask);
  }
};

class csav_list_widget
{
protected:
  ImGui::FileBrowser open_dialog;
  loading_bar_job_widget open_job;
  std::filesystem::path open_filepath;
  std::shared_ptr<csav> opened_save;
  std::shared_ptr<AppImage> opened_save_img;

  std::list<csav_collapsable_header> m_list;

public:
  csav_list_widget()
  {
    open_dialog.SetTitle("Opening savefile");
    open_dialog.SetTypeFilters({ ".dat" });

    // todo: should be a static json loaded on startup and saved on cleanup
    try
    {
      nlohmann::json jpsconfig;
      std::ifstream ifile;
      ifile.open("persistent.json");
      ifile >> jpsconfig;
      ifile.close();
      if (jpsconfig.find("open_path") != jpsconfig.end())
        open_dialog.SetPwd(jpsconfig.at("open_path").get<std::string>());
    }
    catch (std::exception&)
    {
    }
  }
  
  void update()
  {
    open_job.update();
    if (opened_save)
    {
      m_list.emplace_back(opened_save, opened_save_img);
      opened_save.reset();
      opened_save_img.reset();
    }

    m_list.erase(
      std::remove_if(m_list.begin(), m_list.end(), [](auto& a){ return a.is_closed(); }),
      m_list.end()
    );

    for (auto& cs : m_list)
      cs.update();
  }

  void draw_list()
  {
    // todo: remove that when error popup is app-wide
    node_editor_widget::draw_popups();

    // Expose a couple of the available flags. In most cases you may just call BeginTabBar() with no flags (0).
    static ImGuiTabBarFlags tab_bar_flags =
      ImGuiTabBarFlags_Reorderable |
      ImGuiTabBarFlags_AutoSelectNewTabs |
      ImGuiTabBarFlags_FittingPolicyResizeDown;

    // Passing a bool* to BeginTabItem() is similar to passing one to Begin():
    // the underlying bool will be set to false when the tab is closed.
    if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
    {
      if (ImGui::TabItemButton("  +  ", ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_NoTooltip) && !open_job.is_running())
        open_dialog.Open();

      size_t i = 0;
      for (auto it = m_list.begin(); it != m_list.end(); ++it)
      {
        scoped_imgui_id _sii(&*it);
        bool opened = true;
        if (ImGui::BeginTabItem(it->pretty_name().c_str(), &opened, ImGuiTabItemFlags_None))
        {
          it->draw();
          ImGui::EndTabItem();
        }
        if (!opened)
          it->close();
      }
      ImGui::EndTabBar();
    }
  }

  void draw_menu_item(IApp* owning_app)
  {
    if (ImGui::MenuItem("Open savefile", 0, false, !open_job.is_running()))
      open_dialog.Open();

    open_dialog.Display();

    if (open_dialog.HasSelected())
    {
      open_filepath = std::filesystem::absolute(open_dialog.GetSelected().wstring());

      // todo: should be a static json loaded on startup and saved on cleanup
      try
      {
        nlohmann::json jpsconfig;
        std::ifstream ifile;
        ifile.open("persistent.json");
        if (ifile.is_open())
        {
          ifile >> jpsconfig;
          ifile.close();
        }
        std::ofstream ofile;
        ofile.open("persistent.json");
        std::filesystem::path dirpath = open_filepath;
        jpsconfig["open_path"] = dirpath.remove_filename().string();
        ofile << jpsconfig.dump(4);
        ofile.close();
      }
      catch (std::exception&)
      {
      }

      auto screenshot_path = open_filepath;
      screenshot_path.replace_filename(L"screenshot.png");
      if (std::filesystem::exists(screenshot_path))
        opened_save_img = owning_app->load_texture_from_file(screenshot_path.string());

      open_job.start([this](float& progress) -> bool {
        auto cs = std::make_shared<csav>();
        if (!cs->open_with_progress(open_filepath, progress))
          return false;
        opened_save = cs;
        return true;
      });
      ImGui::OpenPopup("Loading..##LOAD");

      open_dialog.ClearSelected();
    }

    if (ImGui::BeginMenu("Options"))
    {
      ImGui::Checkbox("use ps4wizard format", &s_use_ps4_weird_format);
      ImGui::Checkbox("dump decompressed data", &s_dump_decompressed_data);
      ImGui::EndMenu();
    }

    if (0 && ImGui::Button("open error"))
      ImGui::OpenPopup("Loading..##LOAD");

    // Always center this window when appearing
    ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool pushed_stylecol = false;
    if (open_job.failed) {
      ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, (ImVec4)ImColor::HSV(0.f, 1.f, 0.6f, 0.35f));
      pushed_stylecol = true;
    }

    if (ImGui::BeginPopupModal("Loading..##LOAD", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
      ImGui::Text("path: %s", open_filepath.string().c_str());

      if (open_job.is_running())
      {
        open_job.draw();
      }
      else if (open_job.failed)
      {
        ImGui::Text("error, couldn't load savefile");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
          ImGui::CloseCurrentPopup();
      }
      else
      {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    if (pushed_stylecol)
      ImGui::PopStyleColor();
  }
};

